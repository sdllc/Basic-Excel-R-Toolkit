
import {Pipe, ConsoleMessage, ConsoleMessageType} from './pipe';
import {clipboard, remote} from 'electron';
const {Menu, MenuItem} = remote;

import {PromptMessage, TerminalImplementation, AutocompleteCallbackType, ExecCallbackType} from './terminal_implementation';
import {RTextFormatter} from './text_formatter';
import { LanguageInterface, RInterface, JuliaInterface } from './language_interface';

import {Splitter, SplitterOrientation, SplitterEvent} from './splitter';
import {TabPanel, TabJustify, TabEventType} from './tab_panel';
import {DialogManager, DialogSpec, DialogButton} from './dialog';
import {PropertyManager} from './properties';
import {MenuUtilities} from './menu_utilities';

import {MuliplexedTerminal} from './multiplexed_terminal';

import {Editor} from './editor';

import * as Rx from "rxjs";
import * as path from 'path';
import { prototype } from 'stream';
import { language } from './julia_language';

const MenuTemplate = require("../data/menu.json");

let property_manager = new PropertyManager("console-settings", {
  terminal: {}, editor: {}, console: {}
});

let properties = property_manager.properties;
window['properties'] = properties;

// create splitter (main layout)

let splitter = new Splitter(
  document.getElementById("main-window"), 
  properties.terminal.orientation || SplitterOrientation.Horizontal, 
  properties.terminal.split || 50);

// dialogs

let dialog_manager = new DialogManager();

// terminals and tabs

let terminals = new MuliplexedTerminal("#terminal-container", "#terminal-tabs");

// language connections. 
// FIXME: parameterize, or make these dynamic. in fact, do that (make them dynamic).

let language_interface_types = [
  RInterface, JuliaInterface
];

// active languages, set by pipe connections

let language_interfaces = [];

// hide cursor when busy.
// update: don't do this. it causes stray errors as the tooltip
// is looking for it. might be better to hide it at the CSS layer.

// handle closing

let allow_close = true; // dev // false;

let Close = function(){
  Promise.all(language_interfaces.map(language_interface => 
    language_interface.Shutdown())).then(() => {
  
    allow_close = true;
    remote.getCurrentWindow().close();
  });
};

// connect/init pipes, languages

let pipe_list = (process.env['BERT_PIPE_NAME']||"").split(";;"); // separator?

setTimeout(() => {
  
  // FIXME: after languages/tabs are initialized, select tab
  // based on stored preferences (is this going to be a long
  // delay? UX)

  Promise.all(pipe_list.map(pipe_name => {
      return new Promise((resolve, reject) => {
        if(pipe_name){
          let pipe = new Pipe();
          let language = "unknown"; // just for reporting
          pipe.Init({pipe_name}).then(() => {
            pipe.SysCall("get-language").then(response => {
              if( response ){
                console.info( "Pipe", pipe_name, "language response:", response );
                language = response.toString();
                let found = language_interface_types.some(interface_class => {
                  if(interface_class.language_name_ === response ){

                    let instance = new interface_class();
                    instance.label_ = language;
                    language_interfaces.push(instance);
                    instance.InitPipe(pipe, pipe_name);
                    terminals.Add(instance);

                    return true;
                  }
                  return false;
                });
                if(found){
                  pipe.RegisterConsoleMessages();
                }
              }
              console.info( "resolving", language)
              resolve();
            });
          }).catch(e => {
            console.error(e);
            resolve();
          });
        }
        else return resolve();
      });
    })
  ).then(() => {
    console.info( "languages complete");
    if( properties.active_tab ){
      console.info( "Activate:", properties.active_tab);
      terminals.Activate(properties.active_tab);
    }
    terminals.active_tab.subscribe(active => {
      if(properties.active_tab !== active) properties.active_tab = active;
    })
  })

}, 1 );

// construct editor

let editor = new Editor("#editor", properties.editor);

// deal with splitter change on drag end 

splitter.events.filter(x => (x === SplitterEvent.EndDrag||x === SplitterEvent.UpdateLayout)).subscribe(x => {
  terminals.UpdateLayout();
  editor.UpdateLayout();
  properties.terminal.split = splitter.split;
  properties.terminal.orientation = splitter.orientation;
});

// construct menus

MenuUtilities.Load(MenuTemplate);

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

  // prefs is now handled by editor (just loads)

  case "main.view.preferences":
    break;

  // ...

  default:
    console.info(event.id);
  }

});

let resize_timeout_id = 0;
window.addEventListener("resize", event => {

  if(resize_timeout_id) window.clearTimeout(resize_timeout_id);
  resize_timeout_id = window.setTimeout(() => {
    terminals.UpdateLayout();
    editor.UpdateLayout();
    resize_timeout_id = 0;
  }, 100);

  // console.info("RS", event);

});

window.addEventListener("keydown", event => {
  if(event.ctrlKey){
    switch(event.key){
    case 'PageUp':
      editor.PreviousTab();
      break;
    case 'PageDown':
      editor.NextTab();
      break;
    default:
      return;
    }
  }
  else return;
  
  event.stopPropagation();
  event.preventDefault();

});



