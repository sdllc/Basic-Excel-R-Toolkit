
import {Pipe, ConsoleMessage, ConsoleMessageType} from '../io/pipe';
import {Pipe2} from '../io/management_pipe';
import {StdIOPipe} from '../io/stdio_pipe';
import { TerminalImplementation } from './terminal_implementation';

/** generic language interface */
export class LanguageInterface {

  // language name. needs to match what comes out of the pipe
  static language_name_: string;

  // label for tab/id
  label_: string;

  // main comms pipe
  pipe_: Pipe;

  // pipe for control, cancel
  management_pipe_: Pipe2;

  // pipe for passthrough stdio
  stdout_pipe_: StdIOPipe;

  // pipe for passthrough stdio
  stderr_pipe_: StdIOPipe;
  
  // shell text colorizer/formatter
  formatter_:any = null;

  // constructor(){} // unless we need it for something

  InitPipe(pipe:Pipe, name:string){ 

    this.pipe_ = pipe;

    // console handler moved to terminal. FIXME: hide pipe, 
    // pass through observables via accessors

    this.pipe_.control_messages.subscribe(message => {
      console.info( `CM (${this.constructor().language_name_})`, message );
      if( message === "shutdown" ){
        //this.terminal_.CleanUp();
        //allow_close = true; // global
        //remote.getCurrentWindow().close();
        console.warn( "ENOTIMPL: remote shutdown");
      } 
    });

  }

  /** 
   * stub for subclasses. we're introducing circular dependencies.
   */
  AttachTerminal(terminal:TerminalImplementation){}

  Shutdown() : Promise<any> {
    console.info( "LISD1");
    return new Promise((resolve, reject) => {
      if(this.management_pipe_){
        // this.management_pipe_.Close(); // no close method?
      }
      // if( this.terminal_ ){ this.terminal_.CleanUp(); }
      if( this.pipe_ ){
        console.info( "LISD1.1");
        this.pipe_.Close().then(() => {
          console.info( "LISD2");
          return resolve();
        }).catch(() => {
          console.info( "LISD3");
          return resolve();
        });
      }
      else {
        console.info( "LISD4");
        return resolve();
      }
    });
  }

  AutocompleteCallback(buffer:string, position:number) : Promise<any> {
    return null;
  }

  ExecCallback(buffer:string) : Promise<any> { 
    return null; 
  }

  BreakCallback() {}

}
