import { LanguageInterface } from './language_interface';
import { RTextFormatter } from './text_formatter';
import { Pipe } from './pipe';
import { Pipe2 } from './pipe2';

/** specialization: R */
export class RInterface extends LanguageInterface {

  static language_name_ = "R";

  formatter_ = new RTextFormatter();

  InitPipe(pipe:Pipe, name:string){
    super.InitPipe(pipe, name);
    this.management_pipe_ = new Pipe2();
    this.management_pipe_.Init({pipe_name: name + "-M"});
  }

  /*
  AutocompleteCallback(buffer:string, position:number) : Promise<any> {
    return new Promise<any>((resolve, reject) => {
      buffer = buffer.replace( /\\/g, '\\\\').replace( /"/g, '\\"' );
      this.pipe_.Internal(`BERTModule:::.Autocomplete("${buffer}",${position})`).then(x => resolve(x));
    });
  }
  */

  async AutocompleteCallback(buffer:string, position:number) {
    buffer = buffer.replace( /\\/g, '\\\\').replace( /"/g, '\\"' );
    let result = await this.pipe_.Internal(`BERTModule:::.Autocomplete("${buffer}",${position})`);
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