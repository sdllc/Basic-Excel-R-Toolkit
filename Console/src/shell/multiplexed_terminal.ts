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

import { TabPanel, TabJustify } from '../ui/tab_panel';
import { TerminalImplementation } from './terminal_implementation';
import { TerminalState } from './terminal_state';
import { LanguageInterface } from './language_interface';
import { ConfigManagerInstance as ConfigManager, ConfigLoadStatus } from '../common/config_manager';

import { remote } from 'electron';
import * as Rx from 'rxjs';

const {Menu, MenuItem} = remote;
const TerminalContextMenu = require("../../data/menus/terminal_context_menu.json");

interface TerminalInstance {
  node_:HTMLElement;
  state_:TerminalState;
  terminal_:TerminalImplementation;
}

/**
 * replacement for multiplexed terminal; back to 
 * multiple instances, but cleaner
 */
export class MultiplexedTerminal {

  private tabs_:TabPanel;
  private instances_:TerminalInstance[] = [];
  private initialized_ = false;
  private active_:TerminalInstance;
  private preferences_:any;

  private active_tab_:Rx.Subject<string> = new Rx.Subject<string>();
  public get active_tab(){ return this.active_tab_; }

  /**
   * 
   */
  constructor(tab_node_selector:string){

    this.tabs_ = new TabPanel(tab_node_selector, TabJustify.left);

    // tab events for activation
    this.tabs_.events.subscribe(event => {
      let label = event.tab.label;
      switch(event.type){
      case "activate":
        console.info("activate", this.initialized_);
        if(this.initialized_){
          this.ActivateInternal(label);
          this.active_tab_.next(label); 
        }
        break;
      case "deactivate":
        break;

      default:
        console.info( "unexpected tab event", event);
      }
    });

    // we get these keys from the active shell
    TerminalImplementation.events.subscribe(x => {
      switch(x.type){
      case "next-tab":
        this.tabs_.Next();
        break;
      case "previous-tab":
        this.tabs_.Previous();
        break;
      }
    });    

    // subscribe to preference changes. the filter ignores the 
    // initial state where preferences = null.

    ConfigManager.filter(x => x.config).subscribe(x => this.SetPreferences(x.config.shell||{}));

  }

  // FIXME: remove in favor of subscription
  SetPreferences(preferences){
    this.preferences_ = preferences;
    this.instances_.forEach(instance => instance.terminal_.ApplyPreferences(preferences));
  }

  /** send command to active terminal */
  SendCommand(command:string, data?:any){
    this.active_.terminal_.SendCommand(command, data);
  }

  /** focus active terminal (pass through) */
  Focus(){
    this.active_.terminal_.Focus();
  }

  /**
   * get instance by label or number. 
   * FIXME: switch to tag
   */
  private Find(label:string|number) : TerminalInstance {
    let instance:TerminalInstance;
    if(typeof(label) === "string"){
      this.instances_.some(item => {
        if(item.state_.language_interface_.label_ === label){
          instance = item;
          return true;
        }
        return false;
      })
    }
    else {
      instance = this.instances_[label]; // by number
    }
    return instance;
  }

  private ActivateInternal(label:string|number = 0){

    let instance = this.Find(label);
    if(!instance) return; // can't 

    if(this.initialized_ && this.active_ === instance ){
      return;
    }
    if(this.active_) this.active_.node_.style.zIndex = "1";

    this.initialized_ = true;
    this.active_ = instance;
    this.active_.terminal_.Focus();
    this.active_.node_.style.zIndex = "100";

  }

  Activate(label:string|number = 0){
    this.ActivateInternal(label);
    this.tabs_.ActivateTab(label);
  }

  UpdateLayout(){
    this.instances_.forEach(instance => instance.terminal_.Resize());
  }

  async CleanUp(){
    return Promise.all(
      this.instances_.map(instance => {
        instance.terminal_.CleanUp();
        return instance.state_.CleanUp();
      }));
  }

  /**
   * add a terminal for the given language. this method handles creating 
   * a child node, assigning a tab, creating the terminal instance and 
   * attaching the language. 
   * 
   * FIXME: we should share context menu/event handler.
   * FIXME: context menu should be parameterized (per-language)
   */
  Add(language_interface:LanguageInterface, activate = false){

    let state = new TerminalState(language_interface);
    this.tabs_.AddTab({label:language_interface.label_});

    let node = document.createElement("div");
    node.classList.add("terminal-child");
    node.style.zIndex = "1";
    this.tabs_.AppendChildNode(node);

    let terminal = new TerminalImplementation(node);
    terminal.Init(this.preferences_, state);

    this.instances_.push({
      node_:node, terminal_:terminal, state_:state
    });

    node.addEventListener("contextmenu", e => {
      
      let event_handler = function(key, target?){
        switch(key){
        case "terminal-save-image":
          this.SaveImageAs(target)
          break;          
        case "terminal-select-all":
          this.SelectAll();
          break;
        case "terminal-copy":
          this.Copy();
          break;          
        case "terminal-paste":
          this.Paste();
          break;          
        case "terminal-clear-shell":
          this.ClearShell();
          break;
        default:
          console.info(arguments);
        }
      };

      let menu_template:Electron.MenuItemConstructorOptions[] = [];
      let tag = e.target['tagName'] || "";
      let class_name = e.target['className'] || "";
      if( /img/i.test(tag) || (/canvas/i.test(tag) && /xterm-annotation/.test(class_name))){
        TerminalContextMenu["terminal-image"]["items"].forEach(item => {
          if( item.type === "separator" ) menu_template.push({type:"separator"});
          else if( item.id ){
            menu_template.push({ label:item.label, accelerator:item.accelerator, click: event_handler.bind(terminal, item.id, e.target) });
          }
        });
      }
      TerminalContextMenu["terminal"]["items"].forEach(item => {
        if( item.type === "separator" ) menu_template.push({type:"separator"});
        else if( item.id ){
          menu_template.push({ label:item.label, accelerator:item.accelerator, click: event_handler.bind(terminal, item.id) });
        }
      });

      Menu.buildFromTemplate(menu_template).popup();

    });

  }

}

