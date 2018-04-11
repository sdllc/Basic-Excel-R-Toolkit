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
const Constants = require("../../data/constants.json");

export interface AlertSpec {

  /** title displayed on the left-hand side */
  title?: string;

  /** main text */
  message: string;

  /** 
   * array of button labels. clicked label will be 
   * returned. defaults to a single OK button. 
   */
  buttons?: string[];

  /** allow escape key */
  escape?: boolean;

  /** allow enter key */
  enter?: boolean;

  /** timeout: 0 or falsy means don't timeout */
  timeout?: number;

}

export interface AlertResult {
  result: "button" | "escape" | "enter" | "timeout" | "link";
  data?: any;
}

const AlertTemplate = `
  <div class='alert_container'>
    <div class='alert_title'><div class='content'></div></div> 
    <div class='alert_message'><div class='content'></div></div> 
    <div class='alert_buttons'><div class='content'></div></div> 
  </div>
`;

/** 
 * one-line mini dialog. alert is always modal. 
 * FIXME: consolidate with dialog
 * FIXME: multiple alerts? stack? [to stack, restructure this class so we 
 * can use multiple instances, and than append (and remove) container
 * nodes for separate alerts. FIXME: what happens with keys?]
 */
export class AlertManager {

  private static container_node_: HTMLElement;

  private key_listener_: EventListenerObject;
  private click_listener_: EventListenerObject;
  private spec_: AlertSpec;
  private alert_node_:HTMLElement;
  private timeout_id_;

  private static EnsureNodes() {
    if (this.container_node_) return;
    this.container_node_ = document.createElement("div");
    this.container_node_.classList.add("alert_overlay");
    this.container_node_.innerHTML = AlertTemplate;
    document.body.appendChild(this.container_node_);
  }
 
  private DelayResolution(resolve:Function, data:AlertResult){
    setTimeout(() => { 
      this.Hide(); 
      resolve(data);
    }, 1 );
  }

  public Test(){
    this.Show({
      title:"Error",
      message:"Something went wrong. Please fix it! <button>OK</button>",
      buttons:["Close"]
    }).then(x => console.info(x));
  }

  /** 
   * shows an alert. returns a promise that will resolve on a button 
   * click or (optionally) pressing escape. FIXME: return key?
   */
  public Show(spec:AlertSpec) : Promise<AlertResult> {

    MenuUtilities.Disable();
    AlertManager.EnsureNodes();

    this.alert_node_ = (AlertManager.container_node_.querySelector(".alert_container") as HTMLElement);

    return new Promise((resolve, reject) => {

      AlertManager.container_node_.querySelector(".alert_title>div.content").textContent = spec.title || "";
      AlertManager.container_node_.querySelector(".alert_message>div.content").innerHTML = spec.message || "";

      let button_text = `<button>${Constants.dialogs.buttons.ok}</button>`;
      if(spec.buttons && spec.buttons.length){
        button_text = spec.buttons.map(label => `<button>${label}</button>`).join("\n");
      }
      AlertManager.container_node_.querySelector(".alert_buttons>div.content").innerHTML = button_text || "";
      AlertManager.container_node_.style.display = "flex";

      setTimeout(() => this.alert_node_.style.opacity = "1", 100);

      this.key_listener_ = {
        handleEvent: (event) => {
          if(spec.escape){
            if ((event as KeyboardEvent).key === "Escape") {
              this.DelayResolution(resolve, { result: "escape", data: null });
            }
          }
          if(spec.enter){
            if ((event as KeyboardEvent).key === "Enter") {
              this.DelayResolution(resolve, { result: "enter", data: null });
            }
          }
        }
      };
      document.addEventListener("keydown", this.key_listener_);

      this.click_listener_ = {
        handleEvent: (event) => {

          let data;
          let target = event.target as HTMLElement;
          
          if( target ){
            if(target.hasAttribute("data-result")) data = target.getAttribute("data-result");
            else data = target.textContent;
          }

          if (/button/i.test(target.tagName || "")) {
            this.DelayResolution(resolve, { result: "button", data });
          }
          else if(/a/i.test(target.tagName || "")){
            event.stopPropagation();
            event.preventDefault();
            this.DelayResolution(resolve, { result: "link", data });
          }

        }
      };
      AlertManager.container_node_.addEventListener("click", this.click_listener_);

      if(spec.timeout){
        this.timeout_id_ = setTimeout(() => {
          this.DelayResolution(resolve, { result: "timeout", data: null });
          this.timeout_id_ = null;
        }, spec.timeout * 1000);
      }

      this.spec_ = spec;

    });

  }

  /** 
   * closes the alert
   */
  private Hide() {

    let transition_end = () => {
      AlertManager.container_node_.style.display = "none";
      this.alert_node_.removeEventListener("transitionend", transition_end);
    };

    this.alert_node_.addEventListener("transitionend", transition_end);
    this.alert_node_.style.opacity = "0";

    if (this.key_listener_) {
      document.removeEventListener("keydown", this.key_listener_);
      this.key_listener_ = null;
    }
    if (this.click_listener_) {
      AlertManager.container_node_.removeEventListener("click", this.click_listener_);
    }
    if(this.timeout_id_){
      clearTimeout(this.timeout_id_);
      this.timeout_id_ = null;
    }

    this.spec_ = null;
    MenuUtilities.Enable();
  }

}

