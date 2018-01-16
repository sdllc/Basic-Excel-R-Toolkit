
import {Pipe, ConsoleMessage, ConsoleMessageType} from './pipe';
import {Pipe2} from './pipe2';
import {clipboard, remote} from 'electron';
const {Menu, MenuItem} = remote;

import {PromptMessage, TerminalImplementation, TerminalConfig} from './terminal-implementation';
import {RTextFormatter} from './text-formatter';
import {Splitter, SplitterOrientation, SplitterEvent} from './splitter';
import {TabPanel, TabJustify, TabEventType} from './tab-panel';

import {PropertyManager} from './properties';

import {MenuUtilities} from './menu_utilities';

import {Editor} from './editor';

import * as Rx from "rxjs";
import * as path from 'path';
import { prototype } from 'stream';

// properties

let property_manager = new PropertyManager("bert2-console-settings", {
  terminal: {}, editor: {}, console: {}
});

let properties = property_manager.properties;
window['properties'] = properties;

// create splitter (main layout)

let splitter = new Splitter(
  document.getElementById("main-window"), 
  properties.terminal.orientation || SplitterOrientation.Horizontal, 
  properties.terminal.split || 50);

// language connections

let RInterface = {

  label: "R",
  pipe: new Pipe(),
  management_pipe: new Pipe2(),

  autocomplete_callback: function(buffer:string, position:number){
    return new Promise<any>((resolve, reject) => {
      buffer = buffer.replace( /\\/g, '\\\\').replace( '"', '\\"' );
      RInterface.pipe.Internal(`BERTModule:::.Autocomplete("${buffer}",${position})`).then(x => resolve(x));
    });
  },
  
  /**
   * response to a shell command is the next prompt. in some cases, 
   * if the shell enters a (debug) browser, then there is no new 
   * prompt at the end of execution, but we shift back up to the 
   * previous prompt.
   * @param buffer 
   */
  exec_callback: function(buffer:string) : Promise<any>{
    return new Promise((resolve, reject) =>{
      RInterface.pipe.ShellExec(buffer).then(result => {
        if( result === -1 ){ resolve({ pop_stack: true }); }
        else resolve({ text: result });
      });
    });
  },

  break_callback: function(){
    RInterface.management_pipe.SendMessage("break");
  }

}
window['R'] = RInterface;

let JuliaInterface = {
  label: "Julia",
  pipe: new Pipe(),

  exec_callback: function(buffer:string) : Promise<any>{
    return new Promise((resolve, reject) => {
      console.info("CC", buffer);
      JuliaInterface.pipe.ShellExec(buffer).then(result => {
        if( result === -1 ){ resolve({ pop_stack: true }); }
        else resolve({ text: result });
      });
    });
  }

}
window['Julia'] = JuliaInterface;

// terminal tabs

let terminal_tabs = new TabPanel("#terminal-tabs", TabJustify.left);
terminal_tabs.AddTabs(
  {label:"Shell" }
);

// create terminal

let terminal_node = document.getElementById("terminal-container");

let terminal = new TerminalImplementation({
  node_: terminal_node,
  autocomplete_callback_: RInterface.autocomplete_callback,
  exec_callback_: RInterface.exec_callback,
  break_callback_: RInterface.break_callback,
  formatter_: new RTextFormatter()
});

/*
let terminal = new TerminalImplementation({
  node_: terminal_node,
  autocomplete_callback_: null, // RInterface.autocomplete_callback,
  exec_callback_: JuliaInterface.exec_callback,
  break_callback_: null, // RInterface.break_callback,
  formatter_: null, // formatter_: new RTextFormatter()
});
*/

terminal.Init();

terminal_node.addEventListener("contextmenu", e => {
  Menu.buildFromTemplate([
    { label: "Copy", click: () => { terminal.Copy(); }},
    { label: "Paste", click: () => { terminal.Paste(); }},
    { type: "separator" },
    { label: "Clear Shell", click: () => { terminal.ClearShell(); }}
  ]).popup();
});

// bind events

// hide cursor when busy.
// update: don't do this. it causes stray errors as the tooltip
// is looking for it. might be better to hide it at the CSS layer.

/**
 * "busy" overlay implemented as factory class
 */
export class BusyOverlay {
  static Create(
    subject:Rx.Subject<boolean>,
    node_:HTMLElement,
    className:string,
    delay = 250
    ){
      let timeout:any = 0;
      subject.subscribe(state => {
      if(state){
        if(timeout) return;
        timeout = setTimeout(() => {
          terminal_node.classList.add(className);
        }, delay);
      }
      else {
        if(timeout) clearTimeout(timeout);
        timeout = 0;
        terminal_node.classList.remove(className);
      }
    });
  }
}

//BusyOverlay.Create(RInterface.pipe.busy_status_, terminal_node, "busy");
BusyOverlay.Create(JuliaInterface.pipe.busy_status_, terminal_node, "busy");

RInterface.pipe.console_messages.subscribe((console_message) => {
  if( console_message.type === ConsoleMessageType.PROMPT ){
    console.info(`Prompt (${console_message.id})`, console_message.text);
    terminal.Prompt({
      text: console_message.text,
      push_stack: console_message.id !== 0 // true
    });
  }
  else {
    terminal.PrintConsole(console_message.text, !RInterface.pipe.busy);
  }
});

JuliaInterface.pipe.console_messages.subscribe((console_message) => {
  if( console_message.type === ConsoleMessageType.PROMPT ){
    console.info(`(J) Prompt (${console_message.id})`, console_message.text);
    terminal.Prompt({
      text: console_message.text,
      push_stack: console_message.id !== 0 // true
    });
  }
  else {
    terminal.PrintConsole(console_message.text, !RInterface.pipe.busy);
  }
});

let allow_close = true; // dev // false;

RInterface.pipe.control_messages.subscribe(message => {
  console.info( "CM", message );
  if( message === "shutdown" ){
    terminal.CleanUp();
    allow_close = true;
    remote.getCurrentWindow().close();
  } 
});

JuliaInterface.pipe.control_messages.subscribe(message => {
  console.info( "CM (J)", message );
  if( message === "shutdown" ){
    terminal.CleanUp();
    allow_close = true;
    remote.getCurrentWindow().close();
  } 
});


let Close = function(){
  terminal.CleanUp();
  RInterface.pipe.Close().then(() => {
    allow_close = true;
    remote.getCurrentWindow().close();
  }).catch(err => {
    allow_close = true;
    remote.getCurrentWindow().close();
  })
};

// connect/init 

console.info(process);
let pipe_name = process.env['BERT_PIPE_NAME'] || "BERT2-PIPE-R";

setTimeout(() => {

  RInterface.pipe.Init({pipe_name}).then( x => {
    window.addEventListener("beforeunload", event => {
      if(allow_close) return;
      event.returnValue = false;
      setImmediate(() => Close());
    });

    RInterface.management_pipe.Init({pipe_name: pipe_name + "-M"});
    window['mp'] = RInterface.management_pipe;

  }).catch( e => console.info( "error", e ));

  /*
  JuliaInterface.pipe.Init({pipe_name: "BERT2-PIPE-J"}).then(() => {
    console.info( "Pipe init OK?" );
  }).catch(e => console.error(e));
  */

}, 1 );

// construct editor

let editor = new Editor("#editor", properties.editor);
window['E'] = editor;

// deal with splitter change on drag end 

splitter.events.filter(x => (x === SplitterEvent.EndDrag||x === SplitterEvent.UpdateLayout)).subscribe(x => {
  terminal.Resize();
  editor.UpdateLayout();
  properties.terminal.split = splitter.split;
  properties.terminal.orientation = splitter.orientation;
});

// construct menus

MenuUtilities.Load(path.join(__dirname, "..", "data/menu.json")).catch( e => {
  console.info("menu load error: ", e);
});

MenuUtilities.events.subscribe(event => {

  switch(event.id){

  // dev

  case "main.view.reload":
    remote.getCurrentWindow().reload();
    break;
  case "main.view.toggle-developer-tools":
    remote.getCurrentWindow()['toggleDevTools']();
    break;

  // layout

  case "main.view.layout.layout-horizontal":
    splitter.orientation = SplitterOrientation.Horizontal;
    break;
  case "main.view.layout.layout-vertical":
    splitter.orientation = SplitterOrientation.Vertical;
    break;
  default:
    console.info(event.id);
  }

});
