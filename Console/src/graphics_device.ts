
import { Pipe } from './pipe';
import { Metrics, FontMetrics } from './fontmetrics';

// I wanted this to be hidden... we should have a translation library
import * as messages from "../generated/variable_pb.js";

enum LineJoin {
  GE_ROUND_JOIN = 1,
  GE_MITRE_JOIN = 2,
  GE_BEVEL_JOIN = 3
};

enum LineCap {
  GE_ROUND_CAP = 1,
  GE_BUTT_CAP = 2,
  GE_SQUARE_CAP = 3
};

/**
 * 
 */

export class GraphicsDevice {

  /** current drawing canvas */
  active_canvas_:HTMLCanvasElement;
  height_:number;
  width_:number;
  context_:CanvasRenderingContext2D;
  terminal_;
  
  font_metrics_ = new FontMetrics(); 

  constructor( terminal, pipe:Pipe ){
    this.terminal_ = terminal;
    pipe.graphics_message_handler = this.GraphicsCommand.bind(this);

    // FIXME: to listen to events we'll need to trap on 
    // document and then calculate hit targets. (...)

  }

  CreateGraphicsNode(height, width){

    this.height_ = height;
    this.width_ = width;

    let canvas = document.createElement("canvas");
    canvas.setAttribute("height", height);
    canvas.setAttribute("width", width);
    canvas.className = "xterm-graphics-node";
    canvas.style.height = `${height}px`;
    canvas.style.width = `${width}px`;
    
    (this.terminal_ as any).InsertGraphic(height, canvas);

    this.active_canvas_ = canvas;
    this.context_ = canvas.getContext('2d');
    this.context_.font = "14px Calibri";
    this.context_.textAlign = "left"; // "center"; // always

    this.font_metrics_.SetFont("Calibri", "14px");
  }

  SetContext(context){
    
    // FIXME: font, style

    this.context_.strokeStyle = `rgba(${context.col.r}, ${context.col.g}, ${context.col.b}, ${context.col.a/255})`;
    this.context_.fillStyle  = `rgba(${context.fill.r}, ${context.fill.g}, ${context.fill.b}, ${context.fill.a/255})`;
    this.context_.lineWidth = context.lwd;

    if ( context.ljoin == LineJoin.GE_ROUND_JOIN ) this.context_.lineJoin = "round";
    else if ( context.ljoin == LineJoin.GE_MITRE_JOIN ) this.context_.lineJoin = "miter";
    else if ( context.ljoin == LineJoin.GE_BEVEL_JOIN ) this.context_.lineJoin = "bevel";
    
    if ( context.lend == LineCap.GE_ROUND_CAP ) this.context_.lineCap  = "round";
    else if ( context.lend == LineCap.GE_BUTT_CAP ) this.context_.lineCap = "butt";
    else if ( context.lend == LineCap.GE_SQUARE_CAP ) this.context_.lineCap = "square";
    
  }

  GraphicsResponse(x?:number[], y?:number[]){

    let response = new messages.CallResponse();
    let consoleMessage = new messages.Console();
    let graphics = new messages.GraphicsCommand();

    if(x) graphics.setXList(x);
    if(y) graphics.setYList(y);

    consoleMessage.setGraphics(graphics);
    response.setConsole(consoleMessage);
    return response;

  }

  GraphicsCommand(message){

    // we need access to the message to get the raw raster data
    // (if it's a raster). in this case maybe we should restructure
    // to skip conversion, as it's probably unecessary?

    let command = message.toObject().graphics;

    // the parameter here is a passed-through PB message. we could 
    // probably simplify.

    switch(command.command){
    case "measure-text":

      // this one is unusual. it comes from a callback;
      // we need to answer, as quickly as possible. 
      // FIXME: cache? lots of this is repetitive.

      this.SetContext(command.context);
      let tm = this.context_.measureText(command.text);
      return this.GraphicsResponse([tm.width]);

    case "font-metrics":

      this.SetContext(command.context);
      let metrics = this.font_metrics_.Measure(command.text);
      return this.GraphicsResponse([metrics.width], [metrics.ascent, metrics.descent]);
    
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
      //console.info(`clip: (${command.xList[0]}, ${command.yList[0]}), (${command.xList[1]}, ${command.yList[1]})`)
      /*
      this.context_.beginPath();
      this.context_.moveTo(command.xList[0], command.yList[0]);
      this.context_.lineTo(command.xList[0], command.yList[1]);
      this.context_.lineTo(command.xList[1], command.yList[1]);
      this.context_.lineTo(command.xList[1], command.yList[0]);
      this.context_.lineTo(command.xList[0], command.yList[0]);
      this.context_.clip();
      */
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
      this.SetContext(command.context);
      this.context_.save();
      
      // I think chrome can handle this
      this.context_.translate(command.xList[0], command.yList[0]);
      //this.context_.translate(Math.round(command.xList[0]), Math.round(command.yList[0]));
      
      if(command.rot) {
        this.context_.rotate(-command.rot * Math.PI / 180);
      }
      let tmp = this.context_.fillStyle;
      this.context_.fillStyle = this.context_.strokeStyle;
      this.context_.fillText(command.text, 0, 0); // command.xList[0], command.yList[0]);
      this.context_.fillStyle = tmp;
      this.context_.restore();
      break;
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
