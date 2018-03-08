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

const Constants = require("../../data/constants.json");

export interface EditorStackMessage {
  text:string;
  tag:any;
}

/**
 * status bar was originally just text, then it became a stack,
 * now it's a tagged stack. some of the cruft in here reflects
 * handling all the types at the same time. 
 * 
 * try to use tagged push/pop where possible.
 */
export class EditorStatusBar {

  /** main status bar node */
  private node_: HTMLElement;

  /** text, left side */
  private label_: HTMLElement;

  /** line/col (right side) */
  private position_: HTMLElement;

  /** language (right side) */
  private language_: HTMLElement;

  /** accessor */
  public get node() { return this.node_; }

  /** accessor for content, not node */
  // public set label(text) { this.label_.textContent = text; }

  /** stack-based (reverse) */
  private stack_:EditorStackMessage[] = [];

  public PushMessage(text:string, tag:any = 0){
    this.stack_.push({text, tag});
    this.label_.textContent = text;
  }

  public PopMessage(tag?:any){

    if(typeof tag === "undefined"){
      // pop last
      if(this.stack_.length) this.stack_ = this.stack_.slice(0,this.stack_.length-1);
    }
    else {
      // pop all with tag
      this.stack_ = this.stack_.filter(item => item.tag !== tag);
    }

    if(this.stack_.length) this.label_.textContent = this.stack_[this.stack_.length-1].text;
    else this.label_.textContent = "";
  }

  /** accessor for content, not node */
  public set language(text) { this.language_.textContent = text; }

  /** accessor for content, not node: pass [line, col] */
  public set position([line, column]) {
    if (null === line || null === column) {
      this.position_.textContent = "";
    }
    else {
      this.position_.textContent = `${Constants.status.line} ${line}, ${Constants.status.column} ${column}`;
    }
  }

  /** show/hide position, for rendered documents */
  public set show_position(show:boolean){
    this.position_.style.display = show ? "" : "none";
  }
  
  constructor() {

    this.node_ = document.createElement("div");
    this.node_.classList.add("editor-status-bar");

    this.label_ = document.createElement("div");
    this.node_.appendChild(this.label_);

    // this node has flex-grow=1 to push other nodes to left and right
    let spacer = document.createElement("div");
    spacer.classList.add("spacer");
    this.node_.appendChild(spacer);

    this.position_ = document.createElement("div");
    this.node_.appendChild(this.position_);

    this.language_ = document.createElement("div");
    this.node_.appendChild(this.language_);

  }

}
