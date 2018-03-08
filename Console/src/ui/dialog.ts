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

import {MenuUtilities} from './menu_utilities';

/**
 * class for showing dialogs, along the lines of bootstrap's modals 
 * 
 * TODO: enable/disable buttons [more general button class?]
 */
const DialogTemplate = `
  <div class='dialog_container'>
    <div class='dialog_header'>
      <div class='dialog_title'></div>
      <div class='dialog_close_x'></div>
    </div>
    <div class='dialog_body'>
    </div>
    <div class='dialog_footer'>
      <div class='dialog_status'></div>
      <div class='dialog_buttons'></div>
    </div>
  </div>
`;

export interface DialogButton {
  text:string;
  data?:any;
  default?:boolean;
}

export interface DialogSpec {

  title?:string;

  body?:string|HTMLElement;

  className?:string;

  status?:string;

  /** buttons, as text strings (text will be returned) */
  buttons?:(string|DialogButton)[];

  /** support the escape key to close the dialog */
  escape?:boolean;

  /** support the enter key to close the dialog */
  enter?:boolean;

}

export interface DialogStatus {
  fulfilled:boolean;
  resolved:boolean;
  rejected:boolean;
}

export type DialogResult = DialogStatus & Promise<any>;

class DialogManager {

  static container_node_:HTMLElement;
  static node_map_:{[index:string]: HTMLElement|HTMLElement[]} = {};

  static EnsureNodes(){

    if(this.container_node_) return;

    let node  = document.createElement("div");
    node.classList.add( "dialog_overlay");

    // ok, this is a little sloppy. create a flat map...
    node.innerHTML = DialogTemplate;
    let map_children = (element) => {
      let children = element.children;
      if(children){
        Array.prototype.forEach.call(children, x => {
          let cls = x.className;
          if( this.node_map_[cls]){
            let entry = this.node_map_[cls];
            if( Array.isArray(entry)){
              entry.push(x);
            }
            else {
              this.node_map_[cls] = [entry, x];
            }
          }
          else this.node_map_[cls] = x;
          map_children(x);
        });
      }
    }
    map_children(node);

    document.body.appendChild(node);
    this.container_node_ = node;
  
  }

  set title(title) { (DialogManager.node_map_.dialog_title as HTMLElement).textContent = title || ""; }

  set body(body:string|HTMLElement) { 
    let container = (DialogManager.node_map_.dialog_body as HTMLElement);
    container.innerHTML = "";
    if(body){
      if( typeof body === "string" ) container.innerHTML = body; 
      else container.appendChild(body);
    }
  }

  set status(text:string){
    let container = (DialogManager.node_map_.dialog_status as HTMLElement);
    container.innerHTML = text||"";
  }

  set className(name:string){
    let container = (DialogManager.node_map_.dialog_container as HTMLElement);
    container.className = "dialog_container " + (name||"");
  }

  set buttons(buttons:(string|DialogButton)[]) {
    let container = DialogManager.node_map_.dialog_buttons as HTMLElement;
    if(!container) return;

    container.textContent = "";

    buttons.forEach((spec, index) => {
      let button = document.createElement("button") as HTMLElement;
      button.classList.add("button");
      if( typeof spec === "string") button.innerText = spec;
      else {
        button.innerText = spec.text;
        if(spec.default) button.classList.add("default");
      }
      button.setAttribute("data-index", String(index))
      container.appendChild(button);
    });

  }

  private pending_result_;
  private pending_resolve_;

  // hold on to active dialog spec for callbacks
  current_dialog_spec_:DialogSpec;

  key_listener_:EventListenerObject;

  click_listener_:EventListenerObject;

  constructor(){
    DialogManager.EnsureNodes();
  }

  private Hide(){
    DialogManager.container_node_.style.display = "none";
    if( this.key_listener_ ){
      document.removeEventListener("keydown", this.key_listener_);
      this.key_listener_ = null;
    }
    if( this.click_listener_ ){
      DialogManager.container_node_.removeEventListener("click", this.click_listener_);
    }
    this.current_dialog_spec_ = null;
    MenuUtilities.Enable();
  }

  public Close(data?:any){
    if(this.pending_result_) this.DelayResolution(this.pending_result_, this.pending_resolve_, {
      reason: "close", data
    })
  }

  private DelayResolution(dialog_result:DialogResult, resolve:Function, data:any){
    setTimeout(() => { 
      this.pending_result_ = null;
      this.pending_resolve_ = null;
      dialog_result.resolved = dialog_result.fulfilled = true;
      this.Hide(); 
      resolve(data);
    }, 1 );
  }

  Test(){
    this.Show({
      title: "Dialog Title",
      escape: true, enter: true,
      body: `Accent dialog.  Text body. <br/><br/>
              Support for HTML, with some basic layout. Perhaps a few<br/> 
              types for specific widgets (we will definitely need a list).
              <br/><br/>
              The dialog should pad out on very long or very tall content<br/>
              (which is good), but that requires doing manual text layout <br/>
              (which is bad). Maybe have some styles with fixed widths.
            `,
      buttons: ["Cancel", {text: "OK", default:true}],
    }).then(x => {
      console.info(x);
    });
  }

  /** 
   * updates dialog with new data. only replaces data that is present.
   * @see Append for full replacement.
   */
  Append(spec:DialogSpec){
    ["title", "body", "buttons", "className", "status"].forEach(key => {
      if(typeof spec[key] !== "undefined" ){
        this[key] = this.current_dialog_spec_[key] = spec[key];
      }
    });
  }

  /** 
   * updates dialog with new data. this is a complete replacement.
   * @see Append for modifying existing.
   */
  Update(spec:DialogSpec){
    this.title = spec.title||"";
    this.body = spec.body||"";
    this.buttons = spec.buttons||[];
    this.className = spec.className;
    this.status = spec.status||"";

    this.current_dialog_spec_ = spec;
  }

  Show(spec:DialogSpec) : DialogResult {
    
    MenuUtilities.Disable();
    this.current_dialog_spec_ = spec;

    let dialog_result = new Promise<any>((resolve, reject) => {

      this.pending_resolve_ = resolve;

      this.title = spec.title||"";
      this.body = spec.body||"";
      this.buttons = spec.buttons||[];
      this.className = spec.className;
      this.status = spec.status||"";

      DialogManager.container_node_.style.display = "flex";

      // listeners are always attached, and reference the 
      // class member dialog spec; that supports modifying 
      // options while open (via update)

      this.key_listener_ = {
        handleEvent: (event) => {
          console.info(event);
          if(this.current_dialog_spec_.escape){
            if((event as KeyboardEvent).key === "Escape" ){
              this.DelayResolution(dialog_result as DialogResult, resolve, {reason: "escape_key", data: null });
            }
          }
          if(this.current_dialog_spec_.enter){
            if((event as KeyboardEvent).key === "Enter" ){
              this.DelayResolution(dialog_result as DialogResult, resolve, {reason: "enter_key", data: null });
            }
          }
        }
      };
      document.addEventListener("keydown", this.key_listener_);

      this.click_listener_ = {
        handleEvent: (event) => {
          let target = event.target as HTMLElement;
          if(target.className === "dialog_close_x"){
            this.DelayResolution(dialog_result as DialogResult, resolve, {reason: "x", data: null});
          }
          else if(/button/i.test(target.tagName||"")){
            let index = Number(target.getAttribute("data-index")||0);
            let button = this.current_dialog_spec_.buttons[index];
            let data:any = null;
            if( typeof button === "string" ) data = button;
            else data = button.data||button.text;
            this.DelayResolution(dialog_result as DialogResult, resolve, {reason: "button", data });
          }
        }
      };
      DialogManager.container_node_.addEventListener("click", this.click_listener_);

    }) as DialogResult;

    this.pending_result_ = dialog_result;

    return dialog_result;

  }

}

export const Dialog = new DialogManager();

