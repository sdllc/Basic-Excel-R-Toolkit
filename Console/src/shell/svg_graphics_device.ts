/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */

import { Pipe } from '../io/pipe';
import { Metrics, FontMetrics } from './fontmetrics';
import { GraphicsDevice } from './graphics_device';
import { Base64 } from 'js-base64';

import * as fs from 'fs';

// I wanted this to be hidden... we should have a translation library
import * as messages from "../../generated/variable_pb.js";


/**
 * SVG device. for the most part, this is better. the big problem is 
 * with graphs that have lots of nodes; either lots of points or the 
 * raster graphics drawn out of lots of small squares.
 * 
 * that kind of thing kills browsers, even chrome/electron.
 * 
 * it turns out that rendering as an image works much better. at the 
 * very least it's not adding them to the local DOM. that's why we're
 * rendering strings; we never want chrome to have to read these giant
 * node lists. we can do this cleanly, using individual nodes, except 
 * for the container.
 * 
 * there's still some resource utitilization for these things, not 
 * sure exactly where it comes from, but it's livable. there's some 
 * noticeable scroll jitter when one of the images has thousands of nodes.
 */
export class SVGGraphicsDevice extends GraphicsDevice {

  event_id_ = 0;
  tolled_events_ = 0;

  static SVG = "http://www.w3.org/2000/svg";
  
  svg_:string = "<svg>";
  node_:HTMLImageElement;

  CreateGraphicsNode(height, width){

    this.height_ = height;
    this.width_ = width;
    this.node_ = document.createElement("img");

    this.node_.className = "xterm-annotation-node xterm-graphics-node";
    this.node_.style.height = `${height}px`;
    this.node_.style.width = `${width}px`;

    this.InsertGraphic(height, this.node_);
  
  }

  ContextColor(color){
    return `rgba(${color.r}, ${color.g}, ${color.b}, ${color.a/255})`;
  }

  LineAttributes(node:SVGElement, context){

    if ( context.ljoin == GraphicsDevice.LineJoin.GE_ROUND_JOIN ) node.setAttribute("stroke-linejoin", "round");
    else if ( context.ljoin == GraphicsDevice.LineJoin.GE_MITRE_JOIN ) node.setAttribute("stroke-linejoin", "miter");
    else if ( context.ljoin == GraphicsDevice.LineJoin.GE_BEVEL_JOIN ) node.setAttribute("stroke-linejoin", "bevel");
    
    if ( context.lend == GraphicsDevice.LineCap.GE_ROUND_CAP ) node.setAttribute("stroke-linecap", "round"); 
    else if ( context.lend == GraphicsDevice.LineCap.GE_BUTT_CAP ) node.setAttribute("stroke-linecap", "butt");
    else if ( context.lend == GraphicsDevice.LineCap.GE_SQUARE_CAP ) node.setAttribute("stroke-linecap", "square");

    node.setAttribute("stroke-width", context.lwd);
    node.setAttribute("stroke", this.ContextColor(context.col));

    // NOTE: fill is not used here because we don't always set it

  }

  UpdateImage(svg:string = this.svg_){
    if(this.node_) this.node_.src = "data:image/svg+xml;base64," + Base64.encode(svg + "</svg>");
  }

  GraphicsCommand(message, command){

    // we need access to the message to get the raw raster data
    // (if it's a raster). in this case maybe we should restructure
    // to skip conversion, as it's probably unecessary?

    // let command = message.toObject().graphics;
    let node:SVGElement;

    // handle these messages first; they all return immediately.
    // we're also handling new page here, it has slightly 
    // different semantics.

    switch(command.command){
    case "measure-text":
      {
        let weight = (command.context.fontface === 2 || command.context.fontface === 4) ? 600 : 400;
        GraphicsDevice.font_metrics_.SetFont(GraphicsDevice.FontFamily(command.context), 
          GraphicsDevice.PointsToPixels(command.context.ps * command.context.cex), weight);
        let width = GraphicsDevice.font_metrics_.Width(command.text);
        return this.GraphicsResponse([width]);
      }

    case "font-metrics":
      {
        let weight = (command.context.fontface === 2 || command.context.fontface === 4) ? 600 : 400;
        GraphicsDevice.font_metrics_.SetFont(GraphicsDevice.FontFamily(command.context), 
          GraphicsDevice.PointsToPixels(command.context.ps * command.context.cex), weight);
        let metrics = GraphicsDevice.font_metrics_.Measure(command.text);
        return this.GraphicsResponse([metrics.width], [metrics.ascent, metrics.descent]);
      }

    case "new-page":

      // if there's an old graphic, render before continuing
      if(this.event_id_) {
        window.clearTimeout(this.event_id_);
        this.event_id_ = 0;
        this.UpdateImage();
      }

      this.CreateGraphicsNode(command.yList[0] || 300, command.xList[0] || 400);
      this.svg_ = 
        `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"`
        + ` width="${this.width_}" height="${this.height_}" viewBox="0 0 ${this.width_} ${this.height_}">`;

      // fixme: background color? there's no standard way to do that, 
      // so let's add a rectangle... need to move this to method
        
      node = document.createElementNS(SVGGraphicsDevice.SVG, "rect") as SVGElement;
      node.setAttribute("stroke", "none");
      node.setAttribute("fill", this.ContextColor(command.context.fill));
      node.setAttribute("x", "0");
      node.setAttribute("y", "0");
      node.setAttribute("width", String(this.width_));
      node.setAttribute("height", String(this.height_));
      this.svg_ += node.outerHTML;

      return;

    }

    if(this.event_id_) window.clearTimeout(this.event_id_);

    switch(command.command){
    case "draw-raster":

      // this is async. that shouldn't matter, except that there is a 
      // possibility we will get draw operations out of order, which 
      // would result in incorrect stacking. 

      // I guess the solution would be to implement a graphics command 
      // queue, then have a reader run operations async.

      // it seems like this is crazy, but if we know js is single-threaded,
      // then even async there's no possibility of creating garbage in our
      // string. we can still get out of order, though, so an async op stack
      // is still the way to go.

      // FIXME: keep a static canvas node?

      createImageBitmap(new ImageData(Uint8ClampedArray.from(
          message.getGraphics().getRaster_asU8()), command.xList[1], command.yList[1])).then( bitmap => {

        let canvas = document.createElement("canvas");
        canvas.width = command.xList[1];
        canvas.height = command.yList[1];

        let context = canvas.getContext('2d');
        context.drawImage(bitmap, 0, 0);

        let dataURL = canvas.toDataURL('image/png');
        let node = document.createElementNS(SVGGraphicsDevice.SVG, "image");
        node.setAttribute("href", dataURL);
        node.setAttribute("x", command.xList[0]);
        node.setAttribute("y", command.yList[2] < 0 ? command.yList[0]+ command.yList[2] : command.yList[0]);
        node.setAttribute("width", command.xList[2]);
        node.setAttribute("height", String(Math.abs(command.yList[2])));
        node.setAttribute("preserveAspectRatio", "none");

        this.svg_ += node.outerHTML;
        this.event_id_ = window.setTimeout(() => this.UpdateImage(), 50);

      });
      return;

    case "draw-circle":
      node = document.createElementNS(SVGGraphicsDevice.SVG, "circle") as SVGElement;
      this.LineAttributes(node, command.context);
      node.setAttribute("cx", command.xList[0]);
      node.setAttribute("cy", command.yList[0]);
      node.setAttribute("r", command.r);
      node.setAttribute("fill", this.ContextColor(command.context.fill));
      this.svg_ += node.outerHTML;
      break;

    case "set-clip": // ...
      break;

    case "draw-rect":
      command.xList.sort((a,b) => a-b);
      command.yList.sort((a,b) => a-b);
      node = document.createElementNS(SVGGraphicsDevice.SVG, "rect") as SVGElement;
      this.LineAttributes(node, command.context);
      node.setAttribute("fill", this.ContextColor(command.context.fill));
      node.setAttribute("x", command.xList[0]);
      node.setAttribute("y", command.yList[0]);
      node.setAttribute("width", String(command.xList[1] - command.xList[0]));
      node.setAttribute("height", String(command.yList[1] - command.yList[0]));
      this.svg_ += node.outerHTML;
      break;

    case "draw-polyline":
      node = document.createElementNS(SVGGraphicsDevice.SVG, command.filled ? "polygon" : "polyline") as SVGElement;
      this.LineAttributes(node, command.context);
      node.setAttribute("fill", command.filled ? this.ContextColor(command.context.fill) : "none");
      node.setAttribute("points", command.xList.map((x, i) => `${x}, ${command.yList[i]}`).join( " "));
      this.svg_ += node.outerHTML;
      break;

    case "draw-text":

      // console.info("T", command);

      node = document.createElementNS(SVGGraphicsDevice.SVG, "text") as SVGElement;
      node.setAttribute("x", command.xList[0]);
      node.setAttribute("y", command.yList[0]);
      node.textContent = command.text;

      if(command.context.fontface === 2 || command.context.fontface === 4) node.setAttribute("font-weight", "600");
      if(command.context.fontface === 3 || command.context.fontface === 4) node.setAttribute("font-style", "italic");
      node.setAttribute("font-family", GraphicsDevice.FontFamily(command.context)); 
      node.setAttribute("font-size", GraphicsDevice.PointsToPixels(command.context.ps * command.context.cex) + "px");
      if(command.rot){
        node.setAttribute("transform", `rotate(${-command.rot}, ${command.xList[0]}, ${command.yList[0]})`)
      }
      this.svg_ += node.outerHTML;
      break;

    case "draw-line":
      node = document.createElementNS(SVGGraphicsDevice.SVG, "line") as SVGElement;
      this.LineAttributes(node, command.context);
      node.setAttribute("x1", command.xList[0]);
      node.setAttribute("x2", command.xList[1]);
      node.setAttribute("y1", command.yList[0]);
      node.setAttribute("y2", command.yList[1]);
      this.svg_ += node.outerHTML;
      break;

    default:
      console.info("GC", command);
      break;
    }

    this.event_id_ = window.setTimeout(() => this.UpdateImage(), 50);
  
    return null;
  }

}
