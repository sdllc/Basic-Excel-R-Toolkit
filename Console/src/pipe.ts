
import * as fs from "fs";
import * as net from "net";
import * as messages from "../generated/variable_pb.js";
import * as Rx from "rxjs";

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
  PROMPT
}

export interface ConsoleMessage {
  id:number;
  text:string;
  type:ConsoleMessageType;
}

export class Pipe {

  /** Variable (protobuf type) to js object */
  VariableToObject(x) {

    switch (x.getValueCase()) {
      case messages.Variable.ValueCase.NIL:
        return null;
      case messages.Variable.ValueCase.NUM:
        return x.getNum();
      case messages.Variable.ValueCase.STR:
        return x.getStr();
      case messages.Variable.ValueCase.BOOLEAN:
        return x.getBoolean();
      case messages.Variable.ValueCase.ARR:

        let list = x.getArr().getDataList();

        // check for names. if no names, return an array
        let names = list.some(element => element.getName());
        if (!names) return list.map(x => this.VariableToObject(x));

        // if names, return an object. there may be unnamed items, use $X
        let object = {};
        list.forEach((entry, i) => {
          object[entry.getName() || `$${i}`] = this.VariableToObject(entry);
        });
        return object;

      default:
        console.info("UNTRANSLATED\n", x);
        return null;
    }
  }

  /** js intrinsic or object to protobuf Variable */
  ObjectToVariable(x) {
    let type = typeof (x);
    let v = new messages.Variable();
    if (null === x) v.setNil(true);
    else {
      switch (type) {
        case "number":
          v.setNum(x);
          break;
        case "string":
          v.setStr(x);
          break;
        case "boolean":
          v.setBoolean(x);
          break;
      }
    }
    return v;
  }

  /** id for transactions */
  private transaction_id_ = 1;

  /** current pending transaction(s) */
  private pending_: {[index:number]: QueuedCommand} = {};

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
  public busy_status_ = new Rx.BehaviorSubject<boolean>(false);

  /**
   * observable for console messages
   */
  private console_messages_ = new Rx.Subject<ConsoleMessage>();

  /** accessor */
  get console_messages(): Rx.Subject<ConsoleMessage> {
    return this.console_messages_;
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

  private Queue(command, channel) {
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

    // console.info("process queue");

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

    switch (message.channel) {
      case Channel.INTERNAL:
        let code = new messages.Code();
        code.setLineList([message.command]);
        call.setCode(code);
        break;
      case Channel.SYSTEM:
        call.setControlMessage(message.command);
        break;
      case Channel.SYSCALL:
        let function_call = new messages.CompositeFunctionCall;
        function_call.setTarget(messages.CallTarget.SYSTEM);
        function_call.setFunction("get-language");
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
    frame.set(new Uint8Array(frame_length), 0);
    frame.set(data, 4);

    this.client_.write(Buffer.from(frame as any)); // ts type is wrong?

  }

  private ControlMessageCallback(message){
    this.control_messages_.next(message);    
  }

  private ConsoleCallback(response) {
    let obj = response.getConsole();
    switch (obj.getMessageCase()) {
      //case messages.Console.MessageCase.CONTROL_MESSAGE:
      //  this.console_messages_.next({ id:response.getId(), type: ConsoleMessageType.CONTROL_MESSAGE, text: obj.getControlMessage() });
      //  break;
      case messages.Console.MessageCase.PROMPT:
        this.console_messages_.next({ id:response.getId(), type: ConsoleMessageType.PROMPT, text: obj.getPrompt() });
        break;
      case messages.Console.MessageCase.TEXT:
        this.console_messages_.next({ id:response.getId(),type: ConsoleMessageType.TEXT, text: obj.getText() });
        break;
      case messages.Console.MessageCase.ERR:
        this.console_messages_.next({ id:response.getId(),type: ConsoleMessageType.ERR, text: obj.getErr() });
        break;
    }
  }

  private Reject(pending:QueuedCommand, message:any){
    let id = pending.id;
    this.pending_[id] = null; // delete?
    setImmediate(() => pending.reject(message));
  }

  private Resolve(pending:QueuedCommand, message:any){
    let id = pending.id;
    this.pending_[id] = null; // delete?
    setImmediate(() => pending.resolve(message));
  }

  private HandleData(data) {

    let resolve = false;
    try {

      let array = new Uint8Array(data.buffer);
      let stack = [];

      // FIXME: there may be partial packets, in that case we need 
      // to keep a buffer around

      while (array && array.length) {
        let byte_length = new Int32Array(array.buffer.slice(0, 4))[0];
        let response = messages.CallResponse.deserializeBinary(array.slice(4, byte_length + 4));
        stack.push(response);
        array = array.slice(byte_length + 4);
      }

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
            //else if(pending && (message_case === messages.Console.MessageCase.CONTROL_MESSAGE && response.getConsole().getControlMessage() === "reset-prompt")){
            //  this.Resolve(pending, -1);
            //  resolve = true;
            //}
            else setImmediate(() => this.ConsoleCallback(response));
            break;

          case messages.CallResponse.OperationCase.ERR:
            if(pending) this.Reject(pending, response.getErr());
            resolve = true;
            break;

          case messages.CallResponse.OperationCase.RESULT:
            let result = this.VariableToObject(response.getResult());
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
    // if (this.pending_.resolve) setImmediate(() => this.pending_.reject(err));
  }

  /** clean shutdown, notify the service */
  Close(): Promise<any> {

    // flush queue
    this.queue_ = [];

    // push
    return this.Control("close");

  }

  /** initialize and connect to service */
  Init(opts: any = {}) {

    opts = opts || {};
    this.pipe_name_ = opts.pipe_name || "";

    return new Promise((resolve, reject) => {
      console.info("Connecting...");

      let client = net.createConnection({ path: "\\\\.\\pipe\\" + this.pipe_name_ }, () => {
        console.log('connected to service');

        // register for console messages
        setImmediate(() => {
          console.info("calling console");
          this.Control("console").then(() => {
            console.info("registered as console client");
            resolve();
          });
        });
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
