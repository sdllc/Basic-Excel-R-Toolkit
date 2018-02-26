
import { LanguageInterface } from './language_interface';
import {Pipe, ConsoleMessage, ConsoleMessageType} from '../comms/pipe';
import {Pipe2} from '../comms/management_pipe';
import {StdIOPipe} from '../comms/stdio_pipe';

/** specialization: Julia */
export class JuliaInterface extends LanguageInterface {

  static language_name_ = "Julia";
  
  InitPipe(pipe:Pipe, name:string){
    super.InitPipe(pipe, name);
    this.management_pipe_ = new Pipe2();
    this.management_pipe_.Init({pipe_name: name + "-M"});

    this.stdout_pipe_ = new StdIOPipe();
    this.stdout_pipe_.Init({pipe_name: name + "-STDOUT"});

    this.stderr_pipe_ = new StdIOPipe();
    this.stderr_pipe_.Init({pipe_name: name + "-STDERR"});
    
  }

  async AutocompleteCallback(buffer:string, position:number) {

    // FIXME: need to normalize AC data structure (output)

    buffer = buffer.replace( /\\/g, '\\\\').replace( /"/g, '\\"' );
    buffer = buffer.replace( /\$/g, "\\$"); // julia string interpolations

    let x = await this.pipe_.Internal(`BERT.Autocomplete("${buffer}",${position})`);
    let arr:Array<any> = (Array.isArray(x) ? x as Array<any> : [])

    // slightly different breaks (+.). allow leading backslash (escape)
    let token = buffer.replace(/\\\\/g, "\\").replace(/^.*[^\w\\]/, "");
    // console.info( `tok "${token}"`)

    // check if these are function signatures
    if(arr.length && /\(/.test(arr[0])){
      arr = arr.map(x => x.replace(/\)\s+in.*$/, ")" ));

      // sort by number of arguments, then length
      arr.sort((a, b) => {
        let ac = a.replace(/[^,]/g, "").length;
        let bc = b.replace(/[^,]/g, "").length;
        return (ac-bc) || (a.length - b.length);
      });

      return { token, candidates: arr };
    }

    return ({ comps: arr.join("\n"), token });

  }

  async ExecCallback(buffer:string){
    let result = await this.pipe_.ShellExec(buffer);
    if( result === -1 ) return { pop_stack: true };
    return { text: result };
  }

  BreakCallback(){
    this.management_pipe_.SendMessage("break");
  }

}
