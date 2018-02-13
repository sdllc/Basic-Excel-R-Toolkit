
import {PromptMessage, TerminalImplementation, AutocompleteCallbackType, ExecCallbackType} from './terminal_implementation';
import {RTextFormatter} from './text_formatter';
import {LanguageInterface} from './language_interface';
import {TabPanel, TabJustify, TabEventType} from './tab_panel';

import {remote} from 'electron';

const {Menu, MenuItem} = remote;

import * as Rx from "rxjs";

/** data for managing tabbed terminals */
interface TerminalInstance {
  child:HTMLElement;
  terminal:TerminalImplementation;
  layout?:number;
}

/**
 * tabbed interface for multiple shells, supporting different languages.
 * the actual tab component only handles tabs, not content, so we manage
 * displaying/hiding content on tab changes.
 */
export class MuliplexedTerminal {
  
  private terminal_instances_:{[index:string]:TerminalInstance} = {};

  /** currently active instance for context menu, layout */
  private active_instance_:TerminalInstance;

  private tabs_:TabPanel;

  /** layout index increments on update */
  private layout_ = -1;

  /** observable for active tab (by label) */
  private active_tab_ = new Rx.BehaviorSubject<string>(null);

  /** accessor for active tab */
  public get active_tab() { return this.active_tab_; }

  constructor(tab_node_selector:string){

    this.tabs_ = new TabPanel(tab_node_selector, TabJustify.left);

    this.tabs_.events.subscribe(event => {

      let label = event.tab.label;
      let terminal_instance = this.terminal_instances_[label];

      // console.info("tab event", event, event.tab.label);

      switch(event.type){
      case "activate":
        this.active_tab_.next(label); 
        terminal_instance.child.style.display = "block";
        this.active_instance_ = terminal_instance;
        if( terminal_instance.layout !== this.layout_ ) {

          // console.info( " * tab", label, "needs layout update");
          terminal_instance.terminal.Resize();
          terminal_instance.layout = this.layout_;
        }
        terminal_instance.terminal.Focus();
        break;

      case "deactivate":
        terminal_instance.child.style.display = "none";
        if( this.active_instance_ === terminal_instance ) this.active_instance_ = null;
        break;

      default:
        console.info( "unexpected tab event", event);
      }
    });

    TerminalImplementation.events.subscribe(x => {
      switch(x.type){
      case "next-tab":
        this.tabs_.Next();
        this.active_instance_.terminal.Focus();
        break;
      case "previous-tab":
        this.tabs_.Previous();
        this.active_instance_.terminal.Focus();
        break;
      }
    });

  }

  /** focus active terminal (pass through) */
  Focus(){
    this.active_instance_.terminal.Focus();
  }

  Activate(label:string|number = 0){
    this.tabs_.ActivateTab(label);
    this.active_instance_.terminal.Focus();
  }

  /**
   * updates terminal layout for the active terminal, and updates the 
   * internal index. if a new tab is activated and the indexes are different,
   * we assume that the new tab needs a layout refresh.
   */
  UpdateLayout(){

    // increment layout index
    this.layout_++;
    if(!this.active_instance_) return;
    
    // update current/active
    this.active_instance_.terminal.Resize();
    this.active_instance_.layout = this.layout_;
  }

  /**
   * adds a busy overlay (spinning gear). these are tab/terminal specific,
   * because they represent "local" busy and not global busy. timeout is 
   * used to prevent stutter on very short calls (like autocomplete).
   */
  CreateBusyOverlay(subject:Rx.Subject<boolean>, node:HTMLElement, className:string, delay = 250){

    let timeout_id = 0; // captured

    subject.subscribe(state => {
      if(state){
        if(timeout_id) return;
        timeout_id = window.setTimeout(() => {
          node.classList.add(className);
        }, delay);
      }
      else {
        if(timeout_id) window.clearTimeout(timeout_id);
        timeout_id = 0;
        node.classList.remove(className);
      }
    });

  }

  /**
   * clean up resources: call method on child terminal instances
   */
  CleanUp(){
    Object.keys(this.terminal_instances_).forEach(key => {
      this.terminal_instances_[key].terminal.CleanUp();
    });
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

    let child = document.createElement("div");
    child.classList.add("terminal-child");
    this.tabs_.AppendChildNode(child);

    let label = language_interface.label_;
    let terminal = new TerminalImplementation(language_interface, child);
    terminal.Init();

    child.addEventListener("contextmenu", e => {
      Menu.buildFromTemplate([
        { label: "Copy", click: () => { terminal.Copy(); }},
        { label: "Paste", click: () => { terminal.Paste(); }},
        { type: "separator" },
        { label: "Clear Shell", click: () => { terminal.ClearShell(); }}
      ]).popup();
    });
    
    child.style.display = (activate ? "block": "none");

    this.CreateBusyOverlay(language_interface.pipe_.busy_status, child, "busy");

    this.terminal_instances_[label] = {child, terminal};

    // NOTE: we call AddTab **after** creating the local instance, because 
    // we'll get an activation event and we want it to be in the list.

    this.tabs_.AddTab({label});

  }

}
