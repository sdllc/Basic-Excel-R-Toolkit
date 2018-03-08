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

import * as net from "net";
import * as Rx from 'rxjs';

/**
 * third pipe (this is getting a little out of hand) for pass-through
 * stdio, without message wrapping. maybe temp, maybe not. TBD.
 * 
 * FIXME: multiplex stdout/stderr in some fashion, or just use two pipes?
 */

export class StdIOPipe {

  pipe_name_:string;
  client_:any;

  private data_:Rx.Subject<string>;
  public get data() { return this.data_; }

  constructor(){
    this.data_ = new Rx.Subject<string>();    
  }

  HandleError(error){
    console.error(`[3]: ${error}`);
  }

  /** initialize and connect to service */
  Init(opts: any = {}) {

    this.pipe_name_ = opts.pipe_name || "";

    return new Promise((resolve, reject) => {
      console.info("connecting (3)...");

      let client = net.createConnection({ path: "\\\\.\\pipe\\" + this.pipe_name_ }, () => {
        console.log('connected (3)');
      });

      client.on("data", data => this.data_.next(data.toString()));
      client.on("error", err => this.HandleError(err));
      client.on("close", () => this.HandleError("closed"));
      client.on('end', () => {
        console.log('disconnected from pipe (3)');
      });

      this.client_ = client;

    });

  }  
}
