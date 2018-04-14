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

import { Terminal as XTerm, ITerminalOptions, ITheme as ITerminalTheme } from 'xterm';
import { Base64 } from 'js-base64';

//import * as fit from 'xterm/lib/addons/fit/fit';
import * as fit from './custom-fit';
XTerm.applyAddon(fit);

import * as weblinks from 'xterm/lib/addons/webLinks/webLinks';
XTerm.applyAddon(weblinks);

import { wcwidth } from 'xterm/lib/CharWidth';

import * as CursorClientPosition from './cursor_client_position_addon';
XTerm.applyAddon(CursorClientPosition);

import {AnnotationManager, AnnotationType} from './annotation_addon';
XTerm.applyAddon(AnnotationManager);

import { TextFormatter } from './text_formatter';
import { shell, clipboard } from 'electron';
import { LanguageInterface } from './language_interface';
import { Pipe, ConsoleMessage, ConsoleMessageType } from '../io/pipe';

import { ShellHistory, LineInfo, TerminalState } from './terminal_state';
import { Utilities } from '../common/utilities';

const Constants = require("../../data/constants.json");

import * as Rx from 'rxjs';

// for save image dialog
import { remote } from 'electron';

import * as fs from 'fs';

// for julia, replacing backslash entities in the shell like Julia REPL. 
// FIXME: this should be in the language class

const SymbolTable = require('../../data/symbol_table.json');

const BaseTheme:ITerminalTheme = {
  background: "#fff", 
  foreground: "#000",
  selection: "rgba(255, 255, 0, .1)",
  cursor: "#000"
};

const BaseOptions:ITerminalOptions = {
  cols: 80, 
  cursorBlink: true,
  theme: BaseTheme,
  fontFamily: 'consolas',
  fontSize: 13
};

class Autocomplete {

  visible_ = false;
  last_: any;

  private node_:HTMLElement;

  /**
   * 
   */
  constructor(private accept_: Function, private parent_:HTMLElement) {

    this.node_ = document.createElement("ul");
    this.node_.classList.add("terminal-completion-list");
    document.body.appendChild(this.node_);

    // FIXME: do these need to be attached at all times?
    // if not, we could use a single static node

    this.node_.addEventListener("mousemove", e => {
      let node = e.target as HTMLElement;
      if (node.tagName === "A") node = node.parentElement;
      if (node.tagName !== "LI") return;
      if (!node.classList.contains("active")) {
        let nodes = this.node_.querySelectorAll("li");
        for (let i = 0; i < nodes.length; i++) {
          nodes[i].classList.remove("active");
        }
        node.classList.add("active");
      }
    });

    this.node_.addEventListener("click", e => {
      this.Accept();
    });

  }

  /**
   * show autocomplete. if there's a single completion, we want that to
   * populate immediately; but only if it's the initial tab. if we are 
   * showing the list, then as the user types we narrow choices but we
   * don't automatically select the last option (that's what the flag
   * argument is for).
   */
  Show(cursor_position, acceptSingleCompletion = true) {

    if (!this.last_ || !this.last_.comps) {
      this.Dismiss();
      return;
    }

    let comps = this.last_.comps.split(/\n/);
    if( comps && comps.length) comps.sort();

    if( comps.length === 1 && acceptSingleCompletion ){
      this.Dismiss();
      let addition = comps[0].substr(this.last_.token.length);

      // FIXME: this needs to get parameterized to call back to the 
      // language implementation. for now it's OK since R will never 
      // have a symbol that looks like this...

      let scrub = 0;
      if( addition.length === 0 && /^\\/.test(this.last_.token)){
        //addition = HE.decode(`&${this.last_.token.substr(1)};`); 
        addition = SymbolTable.symbols[this.last_.token.substr(1)] || addition;
        scrub = this.last_.token.length;
      }
      
      this.accept_(addition, scrub);
      
      return;
    }

    this.node_.textContent = "";
    comps.forEach((comp, index) => {
      let li = document.createElement("li");
      let a = document.createElement("a");
      a.textContent = comp;
      li.appendChild(a);
      if (!index) li.classList.add("active");
      this.node_.appendChild(li);
    });

    this.node_.scrollTop = 0;

    let top = Math.round(cursor_position.top - this.node_.offsetHeight);
    if (top < 0) top = Math.round(cursor_position.bottom); 

    let left = Math.round(cursor_position.left);

    // FIXME: move to the left if necessary

    this.node_.style.top = `${top}px`;
    this.node_.style.left = `${left}px`;
    this.node_.style.opacity = "1";
    this.visible_ = true;

  }

  Dismiss() {
    this.node_.style.opacity = "0";
    this.visible_ = false;
  }

  Select(dir: number) {
    let nodes = this.node_.querySelectorAll("li");
    let current = -1;
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].classList.contains("active")) {
        nodes[i].classList.remove("active");
        current = i;
        break;
      }
    }
    current += dir;
    if (current < 0) current = nodes.length - 1;
    if (current >= nodes.length) current = 0;

    let node = nodes[current];
    node.classList.add('active');

    let parent = node.parentElement;
    let bottom = node.offsetTop + node.offsetHeight;

    if (bottom > parent.offsetHeight + parent.scrollTop) {
      parent.scrollTop = bottom - parent.offsetHeight;
    }
    else if (node.offsetTop < parent.scrollTop) {
      parent.scrollTop = node.offsetTop;
    }
  }

  Accept() {

    let nodes = this.node_.querySelectorAll("li");
    let accepted = undefined;
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].classList.contains("active")) {
        accepted = nodes[i].textContent;
        break;
      }
    }
    this.Dismiss();

    // console.info("tl?", this.last_, accepted );

    let addition = accepted.substr(this.last_.token.length);
    this.accept_(addition);

  }

  Update(data: any) {
    this.last_ = data;
  }

}

export interface AutocompleteCallbackType { (buffer: string, position: number): Promise<any> }

export interface PromptMessage {
  text?:string;
  push_stack?:boolean;
  pop_stack?:boolean;
}

/**
 * FIXME: enumerated event types
 */
interface TerminalEvent {
  type:string
}

/**
 * implementation of the terminal on top of xtermjs.
 */
export class TerminalImplementation {
 
  private state_:TerminalState; 
  private xterm_: XTerm = null;
  private autocomplete_: Autocomplete;
  private static function_tip_node_:HTMLElement;

  private viewport_rect:DOMRect;

  /** options is constructed from base, then preferences are overlaid */
  private options_:ITerminalOptions; 

  /**
   * we use a static event source for events that are application-global,
   * which may arise out of any existing terminal.
   * 
   * FIXME: I'm not sure about this being static. maybe better to have each
   * instance broadcast events, then have the container (in our case, mux)
   * consolidate, and have anyone else listen to that.
   * 
   */
  private static events_:Rx.Subject<TerminalEvent> = new Rx.Subject<TerminalEvent>();

  /** accessor */
  public static get events() { return this.events_; }

  constructor(private node_:HTMLElement){

    // this.state_ = new TerminalState(language_interface);

    if( !TerminalImplementation.function_tip_node_ ){
      TerminalImplementation.function_tip_node_ = document.createElement("div");
      TerminalImplementation.function_tip_node_.classList.add("terminal-tooltip");
      document.body.appendChild(TerminalImplementation.function_tip_node_);
    }

  }

  ApplyPreferences(preferences){

    // FIXME: would be nice to have deltas here
    // FIXME: this will change layout? (...)

    this.OverlayOptions(this.options_, preferences);

    // Q: what does xterm do with _this_ option object? we know that in the 
    // init methods it takes ownership and modifies it (specifically, it
    // nulls out theme for some reason)

    Object.keys(this.options_).forEach(key => {
      /*
      if( key === "background"){
        console.info("not applying background", this.options_[key]);
      }
      else 
      */
      this.xterm_.setOption(key, this.options_[key]);
    });

    this.UpdateContainerBackground();

  }

  /**
   * returns the theoretical rect around the cursor. 
   * offset is in chars, for pushing back function tips.
   * 
   * @param offset_x 
   */
  CursorClientPosition(offset_x = 0){
    return (this.xterm_ as any).GetCursorPosition(offset_x);
  }

  /** focus */
  Focus(){ 
    this.xterm_.focus(); 
    window['term'] = this; // dev
  }

  CleanUp(){
    // ...
  }

  private RunAutocomplete() {
    
    let thennable = this.state_.language_interface_.AutocompleteCallback(this.state_.line_info.buffer, this.state_.line_info.cursor_position);

    if(thennable) thennable.then(autocomplete_response => {
      if (!autocomplete_response) return;
      this.autocomplete_.Update(autocomplete_response);
      if (this.autocomplete_.visible_) {
        this.autocomplete_.Show(this.CursorClientPosition(), false);
      }
      else {
        // console.info("FT", autocomplete_response);
        this.FunctionTip(autocomplete_response['function.signature'], autocomplete_response.fguess);
      }
    });

  }

  DismissTooltip() {
    this.state_.dismissed_tip_ = this.state_.line_info.cursor_position;
  }

  /**
   * 
   * FIXME: either generalize inputs or move into language-specific classes. 
   * FIXME: support for multiple candidate messages (list, or with up/down?)
   */
  FunctionTip(message?: string, function_guess?: string) {

    if (!message) TerminalImplementation.function_tip_node_.style.opacity = "0";
    else {

      if (message === this.state_.dismissed_tip_) return;

      // FIXME: generalize this into finding position of a character, 
      // we can probably use it again

      // for R functions with environment or list scoping, there can be 
      // dollar signs -- we need to make sure these are interpreted literally

      let regex = new RegExp(function_guess.replace(/\$/g, "\\$") + "\\s*\\(", "i");

      let offset_text = message.replace( /\(.*$/, "X" );
      // console.info( offset_text, offset_text.length )

      this.state_.current_tip_ = message;
      this.state_.dismissed_tip_ = null;

      let node = TerminalImplementation.function_tip_node_;
      node.textContent = message;

      let cursor_bounds = this.CursorClientPosition(-offset_text.length);
      
      node.style.left = `${cursor_bounds.right}px`;
      
      //let client_rect = cursor_node.getBoundingClientRect();
      let top = cursor_bounds.top - node.offsetHeight;

      if (top < 0) { top = cursor_bounds.bottom + 2; }

      node.style.top = `${top}px`;
      node.style.opacity = "1";

    }
  }

  /**
   * left or right delete (by sign)
   * FIXME: multiple? would be handy
   */
  DeleteText(dir:1|-1) {

    let line_info = this.state_.line_info;

    if (dir > 0) { // delete right
      if(line_info.cursor_at_end) return;
      let balance = line_info.right.substr(1);
      this.ClearRight();
      this.Write(balance);
      this.MoveCursor(-LineInfo.MeasureText(balance));
      line_info.set(line_info.left + balance, line_info.cursor_position);
    }
    
    else { // delete left (ruboff)
      if (line_info.cursor_position <= 0) return;
      let balance = line_info.buffer.substr(line_info.cursor_position);
      let w = line_info.CharWidthAt(line_info.cursor_position-1);
      this.Write(`\x1b[${w}D\x1b[0K${balance}`)
      this.MoveCursor(-LineInfo.MeasureText(balance));
      line_info.set(line_info.buffer.substr(0, line_info.cursor_position - 1) + balance, line_info.cursor_position-1);
    }

    this.RunAutocomplete();
  }

  /** 
   * type a character 
   */
  Type(key: string) {
    let line_info = this.state_.line_info;
    if (!line_info.cursor_at_end) {
      this.Write(key);
      this.Write(line_info.right);
      this.MoveCursor(-line_info.char_width_right);
    }
    else {
      this.Write(key);
    }
    line_info.append_or_insert(key);
    this.EnsureCursorVisible();
    this.RunAutocomplete();
  }

  /**
   * in the event we're horizontal scrolling, ensure cursor is on
   * the screen
   */
  EnsureCursorVisible(){

    // FIXME: cache [no]
    let viewport = (this.xterm_ as any).viewport;
    let dimensions = (this.xterm_ as any).renderer.dimensions;
    let screenElement = (this.xterm_ as any).screenElement;
    let viewportElement = (this.xterm_ as any).viewportElement;
    
    let cursor = this.state_.line_info.cursor_position;
    let offset = cursor + this.state_.line_info.prompt.length;

    // FIXME: gate on scrollable, otherwise this is just noise

    if(dimensions.canvasWidth <= this.viewport_rect.width) return;

    // if cursor is at zero, always jump back to the start.
    // actually do that if it's within 1/2 screen width...

    let cursor_position = offset * dimensions.scaledCellWidth;
        
    if( cursor_position <= this.viewport_rect.width/2){
      viewportElement.scrollLeft = 0;
    }

    // and otherwise, scroll right so that cursor is near 1/2 the buffer
    // actually make that 3/4? or based on cursor going l or r? (...)

    else {
      viewportElement.scrollLeft = Math.round(cursor_position - this.viewport_rect.width/2);
    }

    //console.info("SL", cursor, cursor_position, viewport.viewportElement.scrollLeft);
    
  }

  OffsetHistory(dir: number) {
    let line_info = this.state_.line_info;
    let text = this.state_.history_.Offset(dir, line_info.buffer);
    if (text === false) return;
    if (line_info.cursor_position > 0) this.MoveCursor(-line_info.char_width_left);
    this.Escape(`K${text}`);
    line_info.set(text);
  }

  /**
   * 
   */
  Prompt(prompt:PromptMessage) {

    let text = prompt.text;
    let exec_immediately = false;

    this.state_.at_prompt = true;

    if( prompt.push_stack ){

      console.info("push prompt stack: ", JSON.stringify(prompt) );
      this.state_.line_info = new LineInfo(prompt.text);
      this.state_.prompt_stack_.unshift(new LineInfo(prompt.text));

      // move left and clear line before writing
      //this.MoveCursor(-this.state_.line_info.char_width_full_text);
      //this.ClearRight();
      this.Write('\r\n');
 

    }
    else if( prompt.pop_stack ){
      this.state_.line_info = this.state_.prompt_stack_.shift();
    }
    else {

      let pending = this.state_.line_info.buffer || "";

      this.state_.line_info = new LineInfo(prompt.text);

      if( this.state_.pending_exec_list_.length ){
        this.state_.line_info.append(this.state_.pending_exec_list_.shift());
        exec_immediately = true;
      }
      else {

        // this is for the case where you type something while a command 
        // is executing, but you don't press enter. we want the typed text 
        // in front of the next prompt, on a new line.
        // 
        // NOTE: you could just move the prompt in front of the typed 
        // text, but I don't like that effect. probably just because that's
        // not what normal terminals do.

        this.state_.line_info.append(pending);
        if(pending.length) this.Write('\r\n'); 
      }
    }

    this.Write(this.state_.line_info.full_text);
    if( this.state_.line_info.offset_from_end ) {
      // this.MoveCursor(-this.state_.line_info.offset_from_end);
      this.MoveCursor(-this.state_.line_info.char_width_right); // FIXME: CHECK
    }
    this.state_.history_.NewLine();
    
    // handle command if we've been typing while system was busy

    // this is ok w/ state now, with the exception noted above that 
    // other pending commands will get tolled if the state is inactivated
    
    if(exec_immediately){ 
      this.Write('\r\n');
      let line = this.state_.line_info.buffer;
      this.state_.line_info.set(""); 
      this.node_.classList.add("busy"); // should already be busy
      this.state_.Execute(line).then(x => this.Prompt(x));
      this.EnsureCursorVisible();
    }
    else this.node_.classList.remove("busy");

  }

  /** 
   * wrapper for xterm write method. use this one. of course you could call
   * xterm.write directly, we're trying to abstract it a little better
   */
  Write(text: string) {
    this.xterm_.write(text);
  }

  SaveImageAs(target:any){

    let tag = target.tagName || "";
    let image_type = "png";
    let src = null;

    if(/canvas/i.test(tag)){
      src = target.toDataURL("image/png");
      console.info("png");
    }
    else if(/img/i.test(tag)){
      let m = (target.src||"").match(/data\:image\/(.*?)[,;]/);
      if(/svg/i.test(m[1])) {
        image_type = "svg";
        src = target.src;
      }
      else {
        console.info(target);
        image_type = m[1];
        src = target.src;
      }
    }

    if(src){
      let file_name = remote.dialog.showSaveDialog({
        filters: [
          { name:  Constants.shell.imageTypes.replace(/#/, image_type.toUpperCase()), extensions: [image_type] } 
        ]
      });
      if(file_name){
        src = src.replace(/^.*?,/, "");
        if( image_type === "svg" ) fs.writeFile(file_name, Base64.decode(src), "utf8", () => { console.info("image write complete (svg)") });
        else fs.writeFile(file_name, atob(src), "binary", () => { console.info("image write complete (binary)") });
      }
    }
    
  }

  /**
   *
   */
  InsertDataImage(height:number, width:number, mime_type:string, data:Uint8Array, text?:string){

    let node = document.createElement("img") as HTMLImageElement;
    node.className = "xterm-annotation-node xterm-image-node";
    if(width) node.style.width = width + "px";
    if(height) node.style.height = height + "px";

    let src = `data:${mime_type};base64,`;
    if(text) node.src = src + Base64.encode(text);
    else node.src = src + Utilities.Uint8ToBase64(data);
    this.InsertGraphic(height, node);
  }

  /**
   * 
   */
  InsertGraphic(height:number, node:HTMLElement){

    let buffer = (this.xterm_ as any).buffer;
    // console.info(buffer);

    // TAG: switching scaled -> actual to fix highdpi

    let row_height = (this.xterm_ as any).renderer.dimensions.actualCellHeight;
    let rows = Math.ceil( height / row_height ) + 2;
    let row = buffer.y + buffer.ybase + 1;

    this.InsertLines(rows);
    (this.xterm_ as any).annotation_manager.AddAnnotation({
      element: node, line: row
    });

  }
  
  /** 
   * inserts X blank lines above the cursor, preserving prompt 
   * and any existing text. if busy, we can just insert.
   */
  InsertLines(lines:number){
    let offset = !this.state_.language_interface_.pipe_.busy;
    let text = "";
    for( let i = 0; i< lines; i++ ) text += "\n";
    this.PrintConsole(text, offset);
  }

  /**
   * 
   * NOTE: flags is ignored, has been for a while (call it deprecated)
   */
  private PrintConsole(text: string, offset = false) {

    // if not busy, meaning we're waiting at a prompt, we want any
    // stray console messages to appear above (to the left of) the 
    // prompt and push down, preserving prompt and any current line
    // (and cursor pos within the line).

    // this is kind of expensive, so we should look at doing some 
    // batching of console messages, preferably server side but 
    // also client side.

    let lines = text.split(/\n/);
    if (offset) {

      this.MoveCursor(-this.state_.line_info.char_width_full_text);
      this.ClearRight();
      lines.forEach((line, index) => {
        this.Write(this.state_.FormatLine(line));
        if(index !== lines.length-1) this.Write('\r\n');
      });
      this.Write(this.state_.line_info.full_text);
      if(this.state_.line_info.offset_from_end) this.MoveCursor(-this.state_.line_info.char_width_right);
    }
    else {
      lines.forEach((line, index) => {
        this.Write(this.state_.FormatLine(line));
        if(index !== lines.length-1) this.Write('\r\n');
      });
    }
  }

  ClearShell() {
    this.xterm_.clear();
    (this.xterm_ as any).fit();
  }

  /** 
   * FIXME: this needs to be reflected in state... ? 
   */
  ShowCursor(show = true) {
    this.state_.show_cursor_ = show;
    this.Escape(show ? "?25h" : "?25l"); // FIXME: inline
  }

  Escape(command: string) {
    this.Write(`\x1b[${command}`);
  }

  /**
   * xterm abstraction: clear line from cursor right
   */
  ClearRight(){
    this.Escape("K");
  }

  /** 
   * xterm abstraction: move cursor left or right (columns), by sign  
   */
  MoveCursor(columns:number){
    if(!columns) return; 
    let char = columns > 0 ? 'C' : 'D';
    this.Escape(`${Math.abs(columns)}${char}`);
  }

  /** 
   * 
   */
  Copy() {
    let text = this.xterm_.getSelection();
    clipboard.writeText(text);
  }

  /**
   * 
   */
  SelectAll(){
    this.xterm_.selectAll();
  }

  /**
   * 
   */
  Paste(text?: string) {

    this.xterm_.scrollToBottom();
    
    if (!text) text = (clipboard.readText() || "");

    // FIXME: cursor pos [meaning what?]

    let lines = (text || "").split(/\n/).map(x => x.trim());

    lines.reduce((a, line, index) => {
      return new Promise((resolve, reject) => {
        a.then(() => {
          if (lines.length > (index + 1)) {
            this.Write(line + '\r\n'); // writeln
            if (!index) line = this.state_.line_info.buffer + line;
            this.state_.line_info.set("");

            /*
            this.state_.at_prompt = false;
            this.state_.language_interface_.ExecCallback(line).then(x => {
              this.state_.history_.Push(line);
              this.Prompt(x);
              resolve(x);
            });
            */

            this.node_.classList.add("busy");
            this.state_.Execute(line).then(x => {
              this.Prompt(x);
              resolve(x);
            });

          }
          else {
            this.state_.line_info.append_or_insert(line);
            this.Write(line + this.state_.line_info.right);
            this.MoveCursor(-this.state_.line_info.char_width_right);
            resolve();
          }
        });
      });
    }, Promise.resolve());
  }

  KeyDown(key: string, event: any) {

    if (event.ctrlKey && !event.altKey) {
      switch (event.key) {
        case "PageUp":
          TerminalImplementation.events_.next({ type: "previous-tab" });
          break;
        case "PageDown":
          TerminalImplementation.events_.next({ type: "next-tab" });
          break;
        case "e":
          TerminalImplementation.events_.next({ type: "release-focus" });
          break;
        case "a":
          setImmediate(() => { this.SelectAll(); });
          break;
        case "c":
          this.state_.language_interface_.BreakCallback();
          this.state_.pending_exec_list_ = []; // just in case
          break;
        default:
          console.info("ctrl (unhandled):", event);
      }
      this.FunctionTip(); // hide
    }
    else if (event.altKey && !event.ctrlKey) {
      switch(event.key){
        case 'F8':
          this.ClearShell();
          break;
        default:
          console.info("alt (unhandled):", event);
      }
      this.FunctionTip(); // hide
    }
    else {
      let c = key.charCodeAt(0);
      if (c >= 0x20 && c !== 127) {
        this.Type(key);
      }
      else {
        switch (event.key) {

          case "ArrowUp":
            if (this.autocomplete_.visible_) { this.autocomplete_.Select(-1); return; }
            else this.OffsetHistory(-1);
            break;

          case "ArrowDown":
            if (this.autocomplete_.visible_) { this.autocomplete_.Select(1); return; }
            else this.OffsetHistory(1);
            break;

          case "ArrowLeft":
            if (this.state_.line_info.cursor_position > 0) {
              this.Escape(this.state_.line_info.CharWidthAt(this.state_.line_info.cursor_position-1) + "D");
              this.state_.line_info.cursor_position--;
              this.EnsureCursorVisible();
            }
            break;

          case "ArrowRight":
            if(!this.state_.line_info.cursor_at_end){
              this.Escape(this.state_.line_info.CharWidthAt(this.state_.line_info.cursor_position) + "C");
              this.state_.line_info.cursor_position++;
              this.EnsureCursorVisible();
            }
            break;

          case "Escape":
            this.DismissTooltip();
            this.autocomplete_.Dismiss();
            break;

          case "Tab":
            if (this.autocomplete_.visible_) { this.autocomplete_.Accept(); return; }
            this.autocomplete_.Show(this.CursorClientPosition());
            break;

          case "End":
            if(this.state_.line_info.offset_from_end){
              this.MoveCursor(this.state_.line_info.char_width_right);
            }
            this.state_.line_info.cursor_position = this.state_.line_info.buffer.length;
            this.EnsureCursorVisible();
            break;

          case "Home":
            if (this.state_.line_info.cursor_position > 0) {
              this.MoveCursor(-this.state_.line_info.char_width_left);
            }
            this.state_.line_info.cursor_position = 0;
            this.EnsureCursorVisible();
            break;

          case "Backspace":
            this.DeleteText(-1);
            this.EnsureCursorVisible();
            if (this.autocomplete_.visible_) this.RunAutocomplete();
            break;

          case "Delete":
            this.DeleteText(1);
            this.EnsureCursorVisible();
            break;

          case "Enter":
            if (this.autocomplete_.visible_) { this.autocomplete_.Accept(); return; }
            this.Write('\r\n');
            {
              // some updates to exec: (1) we clear buffer immediately. that 
              // way if you type something, it doesn't accidentally get appended
              // to the previous line. (2) we push history _after_ exec, basically
              // only so that history doesn't return itself. 

              // NEW: we want this to work intuitively if you type something
              // while a command is running. with the above fix (1) that works;
              // but it doesn't render to the next prompt before running. I'd 
              // like to do that as well. we can base that on whether we are 
              // currently at prompt. if not, push to a stack. make sure to 
              // flush on break.

              let line = this.state_.line_info.buffer;
              this.state_.line_info.set(""); 

              if(!this.state_.at_prompt){

                // so now we don't exec here, but wait until the next prompt.
                // that works as desired (above). be sure to dump this stack
                // on an interrupt.

                this.state_.pending_exec_list_.push(line);
                return; 
              }

              // FIXME: STATE -- the response may arrive after the terminal
              // has been switched out, so it needs to move to another class
              // (probably the state class?)
              
              /*
              this.state_.at_prompt = false;
              this.state_.language_interface_.ExecCallback(line).then(x => {
                this.state_.history_.Push(line);
                this.Prompt(x);
              });
              */

              this.node_.classList.add("busy");
              this.state_.Execute(line).then(x => this.Prompt(x));
              this.EnsureCursorVisible();

            }
            this.state_.dismissed_tip_ = null;
            break;

          default:
            console.info(c, event.key);
        }
        this.FunctionTip(); // hide

      }

    }

  }

  Resize(){
    
    (this.xterm_ as any).fit();
    this.viewport_rect = (this.xterm_ as any).viewportElement.getBoundingClientRect();

    // FIXME: pass through to languages to adjust terminal size

  }

  /** 
   * the way xterm lays itself out, it will set height to the number of 
   * visible rows * row height. that means there may be a gap at the bottom.
   * if the xterm theme is setting a background color, that gap won't match.
   * 
   * we are creating another gap on the side, via margin, to give some space 
   * between the edge of the window and the text. we could maybe change that
   * but since we have to repair the other gap anyway, we can resolve both 
   * gaps by copying the background color.
   * 
   * note that here we're interrogating xterm rather than using our options,
   * so it should happen after options are set.
   *  
   */
  UpdateContainerBackground(){

    // NOTE that theme is not a property of options. that seems like 
    // an error, and one which may get fixed (watch out). also it's not
    // on the interface.

    // UPDATE: well that doesn't work, because if you update theme, the
    // .theme property is not changed. so use ours.

    let obj = this.xterm_ as any;
    let background = ( this.options_.theme && this.options_.theme.background ) ? this.options_.theme.background : "";

    this.node_.style.background = background;

  }

  /**
   * overlay options from preferences. based on current xterm (and this 
   * could of course change), the structure is one-level deep of 
   * key:value, with a single exception for theme; theme is one-level 
   * deep of key:value.
   */
  OverlayOptions(target:any, src:any){
    Object.keys(src).forEach(key => {
      let target_key_type = typeof(target[key]);
      let src_key_type = typeof(src[key]);
      if( target_key_type === "object" ){
        if( src_key_type === "object" ) {
          this.OverlayOptions(target[key], src[key]);
        }
        else {
          console.warn( "warning: not overlaying target object with src scalar: ", src[key]);
        }
      }
      else { 
        // target is undefined or scalar, overlay
        target[key] = JSON.parse(JSON.stringify(src[key])); 
      }
    });
  }

  SendCommand(command:string, data?:any){
    switch(command){
    case "paste":
      this.Paste(data);
      break;
    case "copy":
      this.Copy();
      break;
    default:
      console.info("unhandled terminal command:", command);
    }
  }

  HandleConsoleMessage(console_message) {

    switch (console_message.type) {

    case ConsoleMessageType.PROMPT:
      if(console_message.data) this.Prompt(console_message.data);
      else this.Prompt({
        text: console_message.text,
        push_stack: console_message.id !== 0 // true
      });
      break;

    case ConsoleMessageType.MIME_DATA:
      if (console_message.data && console_message.data.length) {
        switch (console_message.mime_type) {
          case "text/html":
            let html = new TextDecoder("utf-8").decode(console_message.data);

            // this might be svg, in which case we want to display it as 
            // an image. otherwise it should be html...

            // ...

            if (/\/svg>\s*/i.test(html)) {
              this.InsertDataImage(300, 0, "image/svg+xml", null, html);
            }
            else {
              console.info("UNHANDLED HTML\n", html);
              // window['h'] = html;
            }
            break;

          case "image/jpeg":
          case "image/gif":
          case "image/png":

            //console.info("not rendering");
            // window['msg'] = console_message;
            this.InsertDataImage(300, 0, console_message.mime_type, console_message.data);
            break;
        }
      }
      break;

    default:
      //console.info(`PC (${console_message.text.length})`, console_message.text);
      this.PrintConsole(console_message.text, this.state_.at_prompt);
      break;

    }
  }

  /**
   *
   */
  Init(preferences:any = {}, initial_state:TerminalState) {

    this.state_ = initial_state;

    // we need local options so they don't overlap (for now they're 
    // going to be the same, but in the future we may want to support 
    // per-shell options)

    // see benchmarks; this is the fastest deep copy. 
    
    this.options_ = JSON.parse(JSON.stringify(BaseOptions));
    this.OverlayOptions(this.options_, preferences);

    // NOTE that xterm takes ownership of the options object, and modifies
    // it. that's not necessarily useful to us if we want to keep track.
    // so make another copy.

    let copy = JSON.parse(JSON.stringify(this.options_));

    /*
    if(copy.theme.background){
      console.info("Removing ctb");
      copy.theme.background = "transparent";
    }
    */

    this.xterm_ = new XTerm(copy);

    // so this is for layout. unfortunate.
    let inner_node = document.createElement("div");
    this.node_.appendChild(inner_node);
    this.xterm_.open(inner_node); 
    this.xterm_.focus();

    this.UpdateContainerBackground();
    
    // FIXME: STATE -- have to figure out how to move annotations to state

    // ensure
    (this.xterm_ as any).annotation_manager.Init();

    this.Resize();
    // (this.xterm_ as any).fit();

    // this.xterm_.RebuildLayout();

    //
    // set cursor to vertical bar. otherwise it's a rectangular block, 
    // like the character in adventure. this command should also set blink, 
    // although that's handled separately by xtermjs (as an option)
    //
    this.Write(`\x1b[\x35 q`); 

    let ac_accept = (addition:string, scrub = 0) => {
      if(scrub > 0){
        for( let i = 0; i< scrub; i++ ) this.DeleteText(-1);
      }
      this.Write(addition);
      this.state_.line_info.append_or_insert(addition);      
      this.RunAutocomplete();
    };

    this.autocomplete_ = new Autocomplete(ac_accept, inner_node);

    window.addEventListener("resize", event => {
      // FIXME: debounce
      this.Resize(); // checks active [it does?]
    });

    (this.xterm_ as any).webLinksInit((event: MouseEvent, uri: string) => {
      shell.openExternal(uri);
      return true;
    });

    this.xterm_.on("key", (key, event) => this.KeyDown(key, event));

    // FIXME: I don't think we ever use (or display) title, but it 
    // seems like something that should be part of state. not sure how it 
    // works.

    this.xterm_.on("title", title => {
      console.warn( "title change:", title ); // ??
    });

    // FIXME: since we are no longer swapping state in/out, we don't 
    // need to manage this subscription any longer. not that this has 
    // much cost.
    
    this.state_.console_messages.subscribe(message => {
      this.HandleConsoleMessage(message);
    });

    if(this.state_.stdio){
      this.state_.stdio.subscribe(message => {
        this.PrintConsole(message.text, this.state_.at_prompt);
      });
    }

    // NOTE: this is only used for the graphics devices in R.
    // FIXME: this is backwards, needs a rethink

    this.state_.language_interface_.AttachTerminal(this);

  }

}
