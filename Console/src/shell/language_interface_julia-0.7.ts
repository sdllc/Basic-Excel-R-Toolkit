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

import { LanguageInterface } from './language_interface';
import {Pipe, ConsoleMessage, ConsoleMessageType} from '../io/pipe';
import {Pipe2} from '../io/management_pipe';
import {StdIOPipe} from '../io/stdio_pipe';

import { MenuUtilities } from '../ui/menu_utilities';

/** specialization: Julia */
export class Julia07Interface extends LanguageInterface {

  static language_name_ = "Julia";
  static target_version_ = [ 0, 7, 0 ];

  constructor(){
    super();
    MenuUtilities.events.subscribe(event => {
      switch(event.id){
      case "main.packages.julia.install-packages":
        console.info("Julia install packages");
        break; 
      }
    });
  }

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

    return null;

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
