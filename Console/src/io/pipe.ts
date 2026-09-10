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
import * as Rx from "rxjs";

import { MessageUtilities } from '../common/message_utilities';

enum Channel {
  INTERNAL,
  CALL,
  SYSCALL, 
  SHELL,
  SYSTEM
}

interface QueuedCommand {
  command: string;
  channel: Channel;
  resolve: Function;
  reject: Function;
  id?: number;
}

export enum ConsoleMessageType {
  TEXT,
  ERR,
  PROMPT,
  GRAPHICS,
  MIME_DATA
}

export interface ConsoleMessage {
  id:number;
  text?:string;
  data?:any;
  type:ConsoleMessageType;
  mime_type?:string;
}

export interface HistoryCallbackType { (options?:any): Promise<string[]> }

/**
 * the largest frame we will believe. a big response runs to a few hundred
 * kilobytes; past this the stream is out of step, not merely busy.
 */
const MAX_FRAME_LENGTH = 64 * 1024 * 1024;

export class Pipe {

  /** id for transactions */
  private transaction_id_ = 1;

  /** current pending transaction(s) */
  private pending_: {[index:number]: QueuedCommand} = {};

  /**
   * bytes of a frame that has not arrived in full yet. node hands us at most
   * 64k per read, so a larger response -- a completion list on a loaded
   * workspace, or a long function list -- arrives in pieces.
   */
  private residual_: Buffer = Buffer.alloc(0);

  /** queued commands */
  private queue_: QueuedCommand[] = [];

  /** pipe name */
  private pipe_name_ = "";

  /** pipe connection (nodejs socket) */
  private client_: any;

  /** 
   * observable state. we can't know about actual state, this
   * just reflects whether _we_ (the console client) are running 
   * a command.
   */
  private busy_status_ = new Rx.BehaviorSubject<boolean>(false);

  /** accessor */
  public get busy_status(){ return this.busy_status_; }

  /**
   * observable for console messages
   */
  private console_messages_ = new Rx.Subject<ConsoleMessage>();

  /** accessor */
  get console_messages(): Rx.Subject<ConsoleMessage> {
    return this.console_messages_;
  }

  /** graphics messages are special - they may have responses */
  private graphics_message_handler_:Function;

  set graphics_message_handler(handler:Function){
    this.graphics_message_handler_ = handler;
  }

  /** queries history. has a default implementation. */
  private history_callback_:HistoryCallbackType = async (options:any = {}) => [];

  set history_callback(callback:HistoryCallbackType){
    this.history_callback_ = callback;
  }

  /**
   * observable for control messages
   */
  private control_messages_ = new Rx.Subject<string>();

  /** accessor */
  get control_messages(): Rx.Subject<string>{
    return this.control_messages_;
  }

  /** accessor dereferences subject */
  get busy() { 
    return this.busy_status_.value; 
  }

  constructor() {
 
  }

  Internal(command) {
    return this.Queue(command, Channel.INTERNAL);
  }

  ShellExec(command) {
    return this.Queue(command, Channel.SHELL);
  }

  Control(command) {
    return this.Queue(command, Channel.SYSTEM);
  }

  SysCall(command){
    return this.Queue(command, Channel.SYSCALL);
  }

  private Queue(command, channel) : Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue_.push({
        command, channel, resolve, reject
      });
      if (!this.busy) this.ProcessQueue();
    });
  }

  private SetBusy(busy) {
    this.busy_status_.next(busy);
  }

  private ProcessQueue() {

    if (this.busy) throw ("busy!");

    if (this.queue_.length === 0) {
      // this.pending_ = null;
      return;
    }

    let message = this.queue_.shift();
    message.id = ++this.transaction_id_;

    // console.info("TX message", message.id, message);

    let call = new messages.CallResponse();
    call.setId(message.id);
    call.setWait(true);

    let function_call;

    switch (message.channel) {
      case Channel.INTERNAL:
        let code = new messages.Code();
        code.setLineList([message.command]);
        call.setCode(code);
        break;
      case Channel.SYSTEM:
        function_call = new messages.CompositeFunctionCall;
        function_call.setTarget(messages.CallTarget.SYSTEM);
        function_call.setFunction(message.command);
        call.setFunctionCall(function_call);
        break;
      case Channel.SYSCALL:
        function_call = new messages.CompositeFunctionCall;
        function_call.setTarget(messages.CallTarget.SYSTEM);
        function_call.setFunction(message.command);
        call.setFunctionCall(function_call);
        break;
      case Channel.CALL:
        break;
      case Channel.SHELL:
        call.setShellCommand(message.command);
        break;
    }

    /*
    // if we are not expecting a response, resolve when the write completes

    let callback = call.getWait() ? null :() => {
      this.SetBusy(false);
      if( this.pending_.resolve) setImmediate(() => this.pending_.resolve(null));
    };
    */

    this.pending_[message.id] = message;
    this.SetBusy(true);

    let data = call.serializeBinary();
    let frame_length = new Int32Array(1);
    let frame = new Uint8Array(data.length + 4);

    frame_length[0] = data.length;
    frame.set(new Uint8Array(frame_length.buffer), 0);
    frame.set(data, 4);

    try {
      this.client_.write(Buffer.from(frame as any)); // ts type is wrong?
    }
    catch(e){
      console.error(e);
      this.FlushQueueOnError("write error");
    }

  }

  /**
   * flush queue and any pending responses. all promises will be rejected.
   * generally this indicates that a pipe/language went down. 
   * 
   * FIXME: support re-establishing languages/pipes/connections. 
   * NOTE: when you do that, probably best to dump everything for the 
   * given language and reconnect as if from scratch.
   */
  private FlushQueueOnError(error_message = "pipe error"){
    Object.keys(this.pending_).forEach(key => {
      if( this.pending_[key] ){
        this.pending_[key].reject(error_message);
        this.pending_[key] = null;
        delete this.pending_[key];
      }
    });
    this.queue_.forEach(item => {
      item.reject(error_message);
    })
    this.queue_ = [];
  }

  private ControlMessageCallback(message){
    this.control_messages_.next(message);    
  }

  private ConsoleCallback(response) {
    let obj = response.getConsole();
    switch (obj.getMessageCase()) {
      case messages.Console.MessageCase.PROMPT:
        this.console_messages_.next({ id:response.getId(), type: ConsoleMessageType.PROMPT, text: obj.getPrompt() });
        break;
      
      case messages.Console.MessageCase.TEXT:
        this.console_messages_.next({ id:response.getId(),type: ConsoleMessageType.TEXT, text: obj.getText() });
        break;
      
      case messages.Console.MessageCase.ERR:
        this.console_messages_.next({ id:response.getId(),type: ConsoleMessageType.ERR, text: obj.getErr() });
        break;

      case messages.Console.MessageCase.MIME_DATA:
        this.console_messages_.next({ id:response.getId(),type: ConsoleMessageType.MIME_DATA, mime_type: obj.getMimeData().getMimeType(), data: obj.getMimeData().getData_asU8() });
        break;

      case messages.Console.MessageCase.HISTORY:
        if( this.history_callback_ ){
          this.history_callback_(obj.toObject()).then( history => {

            let response = new messages.CallResponse();
            let variable = MessageUtilities.ObjectToVariable(history);
            response.setResult(variable);

            let data = response.serializeBinary();
            let frame_length = new Int32Array(1);
            let frame = new Uint8Array(data.length + 4);
        
            frame_length[0] = data.length;
            frame.set(new Uint8Array(frame_length.buffer), 0);
            frame.set(data, 4);
        
            this.client_.write(Buffer.from(frame as any)); // ts type is wrong?
          });
        };            
        break;

      case messages.Console.MessageCase.GRAPHICS:

        // graphics messages are specific to R (at least for now), but they
        // are somewhat special in that they may require a response (usually
        // for measuring text). [...]

        // we'll start simple.
        if( this.graphics_message_handler_ ){
          let response = this.graphics_message_handler_(obj); // obj.toObject().graphics);
          if( response ){

            // console.info( "TX response" );

            let data = response.serializeBinary();
            let frame_length = new Int32Array(1);
            let frame = new Uint8Array(data.length + 4);
        
            frame_length[0] = data.length;
            frame.set(new Uint8Array(frame_length.buffer), 0);
            frame.set(data, 4);
        
            this.client_.write(Buffer.from(frame as any)); // ts type is wrong?

          }
        }

        break;
    }
  }

  private Reject(pending:QueuedCommand, message:any){
    let id = pending.id;
    this.pending_[id] = null; // delete?
    delete this.pending_[id];
    setImmediate(() => pending.reject(message));
  }

  private Resolve(pending:QueuedCommand, message:any){
    let id = pending.id;
    this.pending_[id] = null; // delete?
    delete this.pending_[id];
    setImmediate(() => pending.resolve(message));
  }

  private HandleData(data) {

    let resolve = false;
    try {

      // a frame can straddle two packets. this used to parse whatever had
      // arrived, fail, and drop it -- so a response over ~64k never reached
      // the caller, and the shell waited for a reply that was never coming.
      // hold the remainder instead, and wait for the rest of it.

      let buffer: Buffer = this.residual_.length ?
        Buffer.concat([this.residual_, data]) : Buffer.from(data);
      this.residual_ = Buffer.alloc(0);

      let stack = [];
      let offset = 0;

      while (buffer.length - offset >= 4) {

        let byte_length = buffer.readInt32LE(offset);

        // a length we cannot believe means the stream is out of step. there
        // is no way to resynchronise, so drop what we are holding rather than
        // grow a buffer for ever.

        if (byte_length < 0 || byte_length > MAX_FRAME_LENGTH) {
          console.error("dropping console stream: frame length " + byte_length);
          offset = buffer.length;
          break;
        }

        if (buffer.length - offset - 4 < byte_length) break; // the rest is still coming

        let frame = buffer.subarray(offset + 4, offset + 4 + byte_length);

        try {
          stack.push(messages.CallResponse.deserializeBinary(frame));
        }
        catch (e) {

          // a frame the runtime refuses (invalid UTF-8 in a string field,
          // typically). log it and carry on with the next one, so that a
          // single bad message does not take the prompt down with it.

          console.error("dropping console frame:", e.message || e,
            Buffer.from(frame.subarray(0, 260)).toString("hex"));
        }

        offset += 4 + byte_length;
      }

      if (offset < buffer.length) this.residual_ = Buffer.from(buffer.subarray(offset));

      stack.forEach(response => {

        let id = response.getId();
        let pending = this.pending_[id];

        // there are a couple of different cases in which we get a prompt.
        // on startup, there's a first prompt. this arrives along with 
        // (hopefully after) any startup banners.
        
        // next there's a prompt after a command. we treat prompts as the 
        // "result" of console commands, so they resolve the pending 
        // transaction.

        // there's one additional case, where a remote (Excel) command
        // invokes a browser (such as the debugger). in that case we'll 
        // receive an unexpected prompt which takes over execution. we 
        // (the shell) are still sitting on an open prompt.

        // we now handle that via a special "reset" console message that
        // will show up when we exit the browser. this then becomes the 
        // response to the last unexpected prompt...
        
        // [FIXME: need to do this at each level]

        switch (response.getOperationCase()) {

          case messages.CallResponse.OperationCase.FUNCTION_CALL:
            let system_command = response.getFunctionCall().getFunction();
            if( system_command === "reset-prompt" && pending ){
              this.Resolve(pending, -1);
              resolve = true;
            }
            else setImmediate(() => this.ControlMessageCallback(system_command));
            break;

          case messages.CallResponse.OperationCase.CONTROL_MESSAGE:
            let control_message = response.getControlMessage();
            if( control_message === "reset-prompt" && pending ){
              this.Resolve(pending, -1);
              resolve = true;
            }
            else setImmediate(() => this.ControlMessageCallback(control_message));
            break;

          case messages.CallResponse.OperationCase.CONSOLE:
            let message_case = response.getConsole().getMessageCase();

            if (pending && (message_case === messages.Console.MessageCase.PROMPT)) {
              this.Resolve(pending, response.getConsole().getPrompt());
              resolve = true;
            }
            else setImmediate(() => this.ConsoleCallback(response));
            break;

          case messages.CallResponse.OperationCase.ERR:
            if(pending) this.Reject(pending, response.getErr());
            resolve = true;
            break;

          case messages.CallResponse.OperationCase.RESULT:
            let result = MessageUtilities.VariableToObject(response.getResult());
            if(pending) this.Resolve(pending, result);
            resolve = true;
            break;

          default:
            if(pending) this.Resolve(pending, null);
            resolve = true;

        }
      });
    }
    catch (x) {
      console.error(x);
    }

    if (resolve) {
      setImmediate(() => {
        this.SetBusy(false);
        if (!this.busy) this.ProcessQueue()
      });
    }
  };

  private HandleError(err: string | Error) {
    console.error(err);
    this.FlushQueueOnError("pipe error");
  }

  /** clean shutdown, notify the service */
  Close(): Promise<any> {

    // flush queue
    this.queue_ = [];

    // push
    return this.Control("close");

  }

  RegisterConsoleMessages(){
    return new Promise<void>((resolve, reject) => {
      // register for console messages
      console.info("calling console");
      this.Control("console").then(() => {
        console.info("registered as console client");
        resolve();
      });
    });
  }

  /** initialize and connect to service */
  Init(opts: any = {}) {

    opts = opts || {};
    this.pipe_name_ = opts.pipe_name || "";

    return new Promise<void>((resolve, reject) => {
      console.info("Connecting...");

      let client = net.createConnection({ path: "\\\\.\\pipe\\" + this.pipe_name_ }, () => {
        console.log('connected to service');
        resolve();
      });

      client.on("data", data => this.HandleData(data));
      client.on("error", err => this.HandleError(err));
      client.on("close", () => this.HandleError("closed"));
      client.on('end', () => {
        console.log('disconnected from pipe (xmit)');
      });

      this.client_ = client;

    });

  }

}
