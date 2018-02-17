
import { LanguageInterface } from './language_interface';
import { RTextFormatter } from './text_formatter';
import { Pipe } from './pipe';
import { Pipe2 } from './pipe2';

import { GraphicsDevice } from './graphics_device';
import { SVGGraphicsDevice } from './svg_graphics_device';
import { PNGGraphicsDevice } from './png_graphics_device';
import { TerminalImplementation } from './terminal_implementation';

/** specialization: R */
export class RInterface extends LanguageInterface {

  static language_name_ = "R";

  formatter_ = new RTextFormatter();

  InitPipe(pipe:Pipe, name:string){
    super.InitPipe(pipe, name);
    this.management_pipe_ = new Pipe2();
    this.management_pipe_.Init({pipe_name: name + "-M"});
  }

  AttachTerminal(terminal:TerminalImplementation){

    // FIXME: this is language-specific, so it should be in the 
    // language implementation class

    let svg_graphics = new SVGGraphicsDevice();
    let png_graphics = new PNGGraphicsDevice();

    // FIXME: (1) have someone else dispatch. (2) "activate" a device
    // so we don't have to switch on every packet.

    // NOTE: re: (2), if we have to check for a particular message type,
    // then we have to check that in every packet, and so we're testing
    // on every packet anyway.

    this.pipe_.graphics_message_handler = (message) => {
      let command = message.toObject().graphics;
      switch(command.deviceType){
      case "svg":
        return svg_graphics.GraphicsCommand(message, command);
      default:
        return png_graphics.GraphicsCommand(message, command);
      }
    }

    GraphicsDevice.new_graphic.subscribe(x => {
      terminal.InsertGraphic(x.height, x.element);
    });

  }

  async AutocompleteCallback(buffer:string, position:number) {
    buffer = buffer.replace( /\\/g, '\\\\').replace( /"/g, '\\"' );
    let result = await this.pipe_.Internal(`BERT$.Autocomplete("${buffer}",${position})`);
    return result;
  }

  async ExecCallback(buffer:string){
    let result = await this.pipe_.ShellExec(buffer);
    if( result === -1 ) return { pop_stack: true };
    return { text:result };
  }

  /*
  ExecCallback(buffer:string) : Promise<any> {
    return new Promise((resolve, reject) =>{
      this.pipe_.ShellExec(buffer).then(result => {
        if( result === -1 ){ resolve({ pop_stack: true }); }
        else resolve({ text: result });
      }).catch(e => {
        console.error(e);
      });
    });
  }
  */

  BreakCallback(){
    this.management_pipe_.SendMessage("break");
  }

}