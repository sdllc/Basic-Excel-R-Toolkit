
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
