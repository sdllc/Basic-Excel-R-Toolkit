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

// I wanted this to be hidden... we should have a translation library
import * as messages from "../../generated/variable_pb.js";

/**
 * png graphics. has the advantage of being fast, and small,
 * at the expense of detail and resizability. 
 * 
 * [NOTE: GR draws giant pngs and then displays them small. maybe an idea?]
 */
export class PNGGraphicsDevice extends GraphicsDevice {

  /** current drawing canvas */
  active_canvas_:HTMLCanvasElement;

  /** current drawing context */
  context_:CanvasRenderingContext2D;
  
  CreateGraphicsNode(height, width){

    this.height_ = height;
    this.width_ = width;

    let canvas = document.createElement("canvas");
    canvas.setAttribute("height", height);
    canvas.setAttribute("width", width);
    canvas.className = "xterm-annotation-node xterm-graphics-node";
    canvas.style.height = `${height}px`;
    canvas.style.width = `${width}px`;

    this.active_canvas_ = canvas;
    this.context_ = canvas.getContext('2d');
    this.context_.textAlign = "left"; 
    
    this.InsertGraphic(height, canvas);
    
  }

  SetContext(context){
    
    this.context_.strokeStyle = `rgba(${context.col.r}, ${context.col.g}, ${context.col.b}, ${context.col.a/255})`;
    this.context_.fillStyle  = `rgba(${context.fill.r}, ${context.fill.g}, ${context.fill.b}, ${context.fill.a/255})`;
    this.context_.lineWidth = context.lwd;

    if ( context.ljoin == GraphicsDevice.LineJoin.GE_ROUND_JOIN ) this.context_.lineJoin = "round";
    else if ( context.ljoin == GraphicsDevice.LineJoin.GE_MITRE_JOIN ) this.context_.lineJoin = "miter";
    else if ( context.ljoin == GraphicsDevice.LineJoin.GE_BEVEL_JOIN ) this.context_.lineJoin = "bevel";
    
    if ( context.lend == GraphicsDevice.LineCap.GE_ROUND_CAP ) this.context_.lineCap  = "round";
    else if ( context.lend == GraphicsDevice.LineCap.GE_BUTT_CAP ) this.context_.lineCap = "butt";
    else if ( context.lend == GraphicsDevice.LineCap.GE_SQUARE_CAP ) this.context_.lineCap = "square";
    
  }

  GraphicsCommand(message, command){

    // we need access to the message to get the raw raster data
    // (if it's a raster). in this case maybe we should restructure
    // to skip conversion, as it's probably unecessary?

    // let command = message.toObject().graphics;

    // the parameter here is a passed-through PB message. we could 
    // probably simplify.

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
    
    case "draw-raster":

      // this is async. that shouldn't matter, except that there is a 
      // possibility we will get draw operations out of order, which 
      // would result in incorrect stacking. 

      // I guess the solution would be to implement a graphics command 
      // queue, then have a reader run operations async.

      createImageBitmap(new ImageData(Uint8ClampedArray.from(
          message.getGraphics().getRaster_asU8()), command.xList[1], command.yList[1])).then( bitmap => {
        this.context_.drawImage( bitmap, command.xList[0], command.yList[0], command.xList[2], command.yList[2] );
      })
      break;

    case "new-page":
      this.CreateGraphicsNode(command.yList[0] || 300, command.xList[0] || 400);
      break;

    case "draw-circle":
      this.SetContext(command.context);
      this.context_.beginPath();
      this.context_.ellipse(command.xList[0], command.yList[0], command.r, command.r, 0, 0, 2 * Math.PI);
      this.context_.fill();
      this.context_.stroke();
      break;

    case "set-clip":
      break;

    case "draw-rect":
      this.SetContext(command.context);
      this.context_.fillRect(command.xList[0], command.yList[0], command.xList[1] - command.xList[0], command.yList[1] - command.yList[0]);
      this.context_.strokeRect(command.xList[0], command.yList[0], command.xList[1] - command.xList[0], command.yList[1] - command.yList[0]);
      break;

    case "draw-polyline":
      this.SetContext(command.context);
      this.context_.beginPath();
      this.context_.moveTo(command.xList[0], command.yList[0]);
      for( let i = 1; i< command.xList.length; i++ ){
        this.context_.lineTo(command.xList[i], command.yList[i]);
      }
      if(command.filled) this.context_.fill();
      this.context_.stroke();
      break;

    case "draw-text":
    {
      this.SetContext(command.context);
      this.context_.save();
     
      let weight = (command.context.fontface === 2 || command.context.fontface === 4) ? 600 : 400;
      let italic = (command.context.fontface === 3 || command.context.fontface === 4) ? "italic" : "";
      
      let px = GraphicsDevice.PointsToPixels(command.context.ps * command.context.cex);
      this.context_.font = `${italic} ${weight} ${px}px ${GraphicsDevice.FontFamily(command.context)}`

      // I think chrome can handle this (real pixel)
      this.context_.translate(command.xList[0], command.yList[0]);
      
      if(command.rot) {
        this.context_.rotate(-command.rot * Math.PI / 180);
      }
      let tmp = this.context_.fillStyle;
      this.context_.fillStyle = this.context_.strokeStyle;
      this.context_.fillText(command.text, 0, 0); 
      this.context_.fillStyle = tmp;
      this.context_.restore();
      break;
    }

    case "draw-line":
      this.SetContext(command.context);
      this.context_.beginPath();
      this.context_.moveTo(command.xList[0], command.yList[0]);
      this.context_.lineTo(command.xList[1], command.yList[1]);
      this.context_.stroke();
      break;

    default:
      console.info("GC", command);
      break;
    }
    
    return null;
  }

}
