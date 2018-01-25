
// import {PromptMessage, TerminalImplementation, TerminalConfig, AutocompleteCallbackType, ExecCallbackType} from './terminal-implementation';

import {RTextFormatter} from './text_formatter';
import {Pipe, ConsoleMessage, ConsoleMessageType} from './pipe';
import {Pipe2} from './pipe2';

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

  // shell text colorizer/formatter
  formatter_:any = null;

  // constructor(){} // unless we need it for something

  InitPipe(pipe:Pipe, name:string){ 

    this.pipe_ = pipe;

    // console handler moved to terminal. FIXME: hide pipe, 
    // pass through observables via accessors

    this.pipe_.control_messages.subscribe(message => {
      console.info( "CM (${this.name_})", message );
      if( message === "shutdown" ){
        //this.terminal_.CleanUp();
        //allow_close = true; // global
        //remote.getCurrentWindow().close();
        console.warn( "ENOTIMPL: remote shutdown");
      } 
    });

  }

  Shutdown() : Promise<any> {
    return new Promise((resolve, reject) => {
      if(this.management_pipe_){
        // this.management_pipe_.Close(); // no close method?
      }
      // if( this.terminal_ ){ this.terminal_.CleanUp(); }
      if( this.pipe_ ){
        this.pipe_.Close().then(() => {
          resolve();
        }).catch(() => {
          resolve();
        });
      }
      else return resolve();
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

/** specialization: R */
export class RInterface extends LanguageInterface {

  static language_name_ = "R";

  formatter_ = new RTextFormatter();

  InitPipe(pipe:Pipe, name:string){
    super.InitPipe(pipe, name);
    this.management_pipe_ = new Pipe2();
    this.management_pipe_.Init({pipe_name: name + "-M"});
  }

  AutocompleteCallback(buffer:string, position:number) : Promise<any> {
    return new Promise<any>((resolve, reject) => {
      buffer = buffer.replace( /\\/g, '\\\\').replace( '"', '\\"' );
      this.pipe_.Internal(`BERTModule:::.Autocomplete("${buffer}",${position})`).then(x => resolve(x));
    });
  }

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

  BreakCallback(){
    this.management_pipe_.SendMessage("break");
  }

}

/** specialization: Julia */
export class JuliaInterface extends LanguageInterface {

  static language_name_ = "Julia";
  
  ExecCallback(buffer:string) : Promise<any> {
    return new Promise((resolve, reject) => {
      console.info("CC", buffer, this.pipe_.busy);
      this.pipe_.ShellExec(buffer).then(result => {
        if( result === -1 ){ resolve({ pop_stack: true }); }
        else resolve({ text: result });
      }).catch(e => {
        console.error(e);
      })
    });
  }
    
}
