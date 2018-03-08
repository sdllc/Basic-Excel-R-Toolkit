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
import * as messages from "../../generated/variable_pb.js";

/**
 * second pipe for out-of-band management tasks [FIXME: merge]
 * 
 * this class doesn't strictly need formatted messages, but it's not
 * a lot of overhead so we might as well use the same types.
 */
export class Pipe2 {

  pipe_name_:string;
  client_:any;
  transaction_id_ = 1;

  HandleData(data){
    console.info(`[2]: ${data}`);
  }

  HandleError(error){
    console.error(`[2]: ${error}`);
  }

  /**
   * send message. wrapped up in a PB structure, framed.
   */
  SendMessage(message){

    let call = new messages.CallResponse();
    call.setId(this.transaction_id_++);
    call.setWait(false);
    //call.setControlMessage(message);
    let function_call = new messages.CompositeFunctionCall;
    function_call.setTarget(messages.CallTarget.SYSTEM);
    function_call.setFunction(message);
    call.setFunctionCall(function_call);

    console.info(JSON.stringify(call.toObject(), undefined, 2));

    let data = call.serializeBinary();
    let frame_length = new Int32Array(1);
    let frame = new Uint8Array(data.length + 4);

    frame_length[0] = data.length;
    frame.set(new Uint8Array(frame_length), 0);
    frame.set(data, 4);
    this.client_.write(Buffer.from(frame as any)); // ts type is wrong?

  }

  /** initialize and connect to service */
  Init(opts: any = {}) {

    this.pipe_name_ = opts.pipe_name || "";

    return new Promise((resolve, reject) => {
      console.info(`connecting (2: ${this.pipe_name_})...`);

      let client = net.createConnection({ path: "\\\\.\\pipe\\" + this.pipe_name_ }, () => {
        console.info(`connected (2: ${this.pipe_name_})`);
      });

      client.on("data", data => this.HandleData(data));
      client.on("error", err => this.HandleError(err));
      client.on("close", () => this.HandleError("closed"));
      client.on('end', () => {
        console.log('disconnected from pipe (2)');
      });

      this.client_ = client;

    });

  }
}
