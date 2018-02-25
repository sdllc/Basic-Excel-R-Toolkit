
import { LanguageInterface } from './language_interface';
import { ConsoleMessage, ConsoleMessageType } from './pipe';

import * as Rx from 'rxjs';

export enum ConsolePrintFlags {
  None = 0, 
  Error = 1
}

export interface PrintConsoleMessage {
  text:string;
  offset:boolean;
  flags:ConsolePrintFlags;
}

export enum StdIOStream {
  STDIN, STDOUT, STDERR
};

export interface StdIOMessage {
  text:string;
  stream:StdIOStream;
}

// export interface RenderLineFunction { (line: string, lastline: boolean, flags: ConsolePrintFlags ): void }
export interface FormatLineFunction { (text:string): string }

export class ShellHistory {

  private history_: string[] = [];
  private copy_: string[] = [];
  private pointer_ = 0;

  /** accessor */
  public get history(){ return this.history_.slice(0); }

  /** language-specific storage key */
  private key_:string;

  /** 
   * flag: write history on every new line. better for dev, 
   * can unwind for production
   */
  private always_store_ = false;

  constructor(language_label:string, always_store = false){
    this.key_ = "console-history-" + language_label;
    this.always_store_ = always_store;
  }

  Push(line: string) {
    if (line.length) {
      this.history_.push(line);
      if(this.always_store_) this.Store();
    }
  }

  /**
   * write to localstorage
   */
  Store() {
    let json = JSON.stringify(this.history_.slice(0, 1024));
    localStorage.setItem(this.key_, json);
  }

  /** 
   * restore from local storage 
   */
  Restore() {
    let tmp = localStorage.getItem(this.key_);
    if (tmp) this.history_ = JSON.parse(tmp);
  }

  NewLine() {
    if (this.history_.length > 1024) this.history_ = this.history_.slice(this.history_.length - 1024);
    this.copy_ = this.history_.slice(0);
    this.pointer_ = this.copy_.length;
    this.copy_.push("");
  }

  Offset(offset: number, current_buffer: string): (string | false) {
    let index = this.pointer_ + offset;
    if (index < 0 || index >= this.copy_.length) return false;
    let line = this.copy_[index];
    this.copy_[this.pointer_] = current_buffer;
    this.pointer_ = index;
    return line;
  }

}

/**
 * abstracting line info. we want to move operations into this class
 * and use accessors. it breaks down a bit and gets messy when we are 
 * inserting, but it's still worthwhile for the most part.
 */
export class LineInfo {

  private cursor_position_ = 0;
  private buffer_ = "";
  
  /**
   * not sure how I feel about constructor-declared properties. it kinds
   * of obscures them (although if you just rely on tooling, they'll be 
   * visible).
   */
  constructor(private prompt_ = ""){}

  append(text:string){
    this.buffer_ += text;
    this.cursor_position_ += text.length;
  }

  /**
   * append, or if the cursor is not at the end, insert at cursor position.
   * in either case increment cursor position by the length of the added text.
   */
  append_or_insert(text:string){
    this.buffer_ = 
      this.buffer_.substr(0, this.cursor_position_) + text +
      this.buffer_.substr(this.cursor_position);
    this.cursor_position_ += text.length;
  }

  /**
   * replace any current text and cursor position. optionally set position
   * to end of line.
   */
  set(buffer:string, cursor_position = -1){
    if( cursor_position === -1 ) cursor_position = buffer.length;
    this.cursor_position_ = cursor_position;
    this.buffer_ = buffer;
  }

  get offset_from_end(){
    return this.buffer_.length - this.cursor_position_;
  }

  /** accessor */
  get prompt(){ return this.prompt_; }

  /** accessor */
  get buffer(){ return this.buffer_; }

  /** accessor */
  get cursor_position(){ return this.cursor_position_; }

  /** accessor */
  set cursor_position(cursor_position:number){ this.cursor_position_ = cursor_position; }

  /** get the full line of text, including prompt */
  get full_text(){ return this.prompt_ + this.buffer_ }

  /** get text left of the cursor */
  get left(){ return this.buffer_.substr(0, this.cursor_position_); }
  
  /** get text right of the cursor */
  get right(){ return this.buffer_.substr(this.cursor_position_); }

}

/**
 * breaking out state for multiplexing...
 * things which are still going to be problematic: 
 * (1) the pending execution stack; (2) annotations.
 * there's also a bit in the ctor where the language
 * attaches to the terminal, that should change.
 */
export class TerminalState {

  public line_info_ = new LineInfo();
  public history_:ShellHistory; 
  public current_tip_: any;
  public dismissed_tip_: any;
  public prompt_stack_:LineInfo[] = [];
  public at_prompt_ = true; // for various reasons // false;
  public pending_exec_list_:string[] = [];
  public language_interface_:LanguageInterface;

  public FormatLine:FormatLineFunction = (text:string) => text;

  public show_cursor_ = true;

  public Buffers:any; // FIXME: type

  public Save(){}
  public Restore(){}

  private console_messages_:Rx.Observable<ConsoleMessage>;
  public get console_messages(){ return this.console_messages_; }

  private stdio_:Rx.Observable<StdIOMessage>;
  public get stdio(){ return this.stdio_; }
  
  // this is an observable/observer for messages constructed internally, 
  // so they get merged into the overall message stream. the intial use 
  // of this is for exec() responses.

  // private internal_messages_:Rx.Subject<ConsoleMessage> = new Rx.Subject<ConsoleMessage>();

  constructor(language_interface:LanguageInterface){

    this.language_interface_ = language_interface;
    this.history_ = new ShellHistory((this.language_interface_.label_||"").toLocaleLowerCase(), true);
    this.history_.Restore();

    // there's a default implementation, just override if necessary    

    if( this.language_interface_.formatter_ ){
      this.FormatLine = (text:string) => this.language_interface_.formatter_.FormatString(text);
    }

    // returns history. history comes from the history class which is a 
    // member of state, so we don't need to call through the terminal.
    //
    // FIXME: why is this async? 
    //
    this.language_interface_.pipe_.history_callback = async (options?:any) => {
      return this.history_.history || [];
    }

    // since we are no longer swapping state in and out, it might be 
    // more efficient to split these back up (at least the stdio streams)

    this.console_messages_ = Rx.Observable.create(observer => {
      this.language_interface_.pipe_.console_messages.subscribe(observer);
    });

    let sources:any[] = [];
    
    if(this.language_interface_.stdout_pipe_){
      sources.push(this.language_interface_.stdout_pipe_.data.map(text => {
        return { text, stream:StdIOStream.STDOUT }
      }));
    }

    if(this.language_interface_.stderr_pipe_){
      sources.push(this.language_interface_.stderr_pipe_.data.map(text => {
        return { text, stream:StdIOStream.STDERR }
      }));
    }

    // only create this if necessary, client can test

    if(sources.length){
      this.stdio_ = Rx.Observable.create(observer => {
        Rx.Observable.merge(...sources).subscribe(observer);
      });
    }

    // consolidate console messages and stdio streams. we need to preserve 
    // order, the merge operator should do that correctly

    /*
    let sources:any[] = [
      // this.internal_messages_,
      this.language_interface_.pipe_.console_messages
    ];

    if(this.language_interface_.stdout_pipe_){
      sources.push(this.language_interface_.stdout_pipe_.data.map(text => {
        return { text, type:ConsoleMessageType.TEXT }
      }));
    }

    if(this.language_interface_.stderr_pipe_){
      sources.push(this.language_interface_.stderr_pipe_.data.map(text => {
        return { text, type:ConsoleMessageType.ERR }
      }));
    }
    
    / *
    let merged_sources = Rx.Observable.merge(...sources);

    // I'm definitely doing this wrong. need to wrap my head around rx patterns

    let message_cache = [];
    let cache_subscription = merged_sources.subscribe(x => message_cache.push(x));

    this.console_messages_ = Rx.Observable.create(observer => {
      cache_subscription.unsubscribe();
      let tmp = message_cache;
      message_cache = [];
      Rx.Observable.concat(Rx.Observable.from(tmp), merged_sources).subscribe(observer);
      return () => {
        cache_subscription = merged_sources.subscribe(x => message_cache.push(x));
      };
    });
    * /

    this.console_messages_ = Rx.Observable.create(observer => {
      Rx.Observable.merge(...sources).subscribe(observer);
    });
    */

  }

  /**
   * exec function moved here, in case the terminal is switched out
   * (deactivated) before the command returns. in that case, we want
   * to push the result prompt onto cache.
   */
  async Execute(line:string) {

    /*
    // the problem with this is that the returned value here is a 
    // `PromptMessage`, not a `ConsoleMessage`. we need a way to 
    // handle this generically.

    // TEMPORARILY we're going to stuff this into the data field of 
    // a prompt message, and check for that. should pad out the root
    // type though.

    this.at_prompt_ = false;
    this.language_interface_.ExecCallback(line).then(x => {
      this.history_.Push(line);
      this.internal_messages_.next({id: 0, type:ConsoleMessageType.PROMPT, data: x});
    });
    */

    this.at_prompt_ = false;
    let x = await this.language_interface_.ExecCallback(line);
    this.history_.Push(line);
    return x;
  
  }

  /**
   * any housekeeping before closing
   */
  async CleanUp() {
    this.history_.Store();
    return this.language_interface_.Shutdown();
  }

}