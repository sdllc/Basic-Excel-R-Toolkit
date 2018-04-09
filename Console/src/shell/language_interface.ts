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

import {Pipe, ConsoleMessage, ConsoleMessageType} from '../io/pipe';
import {Pipe2} from '../io/management_pipe';
import {StdIOPipe} from '../io/stdio_pipe';
import { TerminalImplementation } from './terminal_implementation';

import * as Rx from 'rxjs';

export interface LanguageNotification {
  type:string;
  data:any;
}

/** generic language interface */
export class LanguageInterface extends Rx.Subject<LanguageNotification> {

  // language name. needs to match what comes out of the pipe
  static language_name_: string;

  // version, basically for julia 0.6 vs 0.7
  static target_version_ = [0, 0, 0];

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

  constructor(){
    super();
  } 
  
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

    // ugh. use async.

    return new Promise((resolve, reject) => {
      if(this.management_pipe_){
        // this.management_pipe_.Close(); // no close method?
      }
      // if( this.terminal_ ){ this.terminal_.CleanUp(); }
      if( this.pipe_ ){
        this.pipe_.Close().then(() => {
          return resolve();
        }).catch(() => {
          return resolve();
        });
      }
      else {
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
