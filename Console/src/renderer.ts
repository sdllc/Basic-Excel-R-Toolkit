
import {Pipe, ConsoleMessage, ConsoleMessageType} from './pipe';
import {Pipe2} from './pipe2';
import {clipboard, remote} from 'electron';
const {Menu, MenuItem} = remote;

import {PromptMessage, TerminalImplementation, TerminalConfig} from './terminal-implementation';
import {RTextFormatter} from './text-formatter';
import {Splitter, SplitterOrientation} from './splitter';
import {TabPanel, TabJustify, TabEventType} from './tab-panel';

import {PropertyManager} from './properties';

import {Editor} from './editor';

import * as Rx from "rxjs";
import * as path from 'path';

const R = new Pipe();
window['R'] = R;

let property_manager = new PropertyManager("bert2-console-settings", {
  terminal: {}, editor: {}, console: {}
});

let properties = property_manager.properties;
window['properties'] = properties;

const management_pipe = new Pipe2();

// create splitter (main layout)

let splitter = new Splitter(
  document.getElementById("main-window"), 
  properties.terminal.orientation || SplitterOrientation.Horizontal, 
  properties.terminal.split || 50);

// terminal tabs

let terminal_tabs = new TabPanel("#terminal-tabs", TabJustify.left);
terminal_tabs.AddTabs(
  {label:"R Console" } // , {label:"Other Console" }
);

// create terminal

let node = document.getElementById("terminal-container");

let autocomplete_callback = function(buffer:string, position:number){
  return new Promise<any>((resolve, reject) => {
    buffer = buffer.replace( /\\/g, '\\\\').replace( '"', '\\"' );
    R.Internal(`BERTModule:::.Autocomplete("${buffer}",${position})`).then(x => resolve(x));

  });
}

// editor tabs

let editor_tabs = new TabPanel("#editor-tabs");
editor_tabs.AddTabs(
  {label:"File1.js", closeable:true, button:true}, 
  {label:"File2.R", closeable:true, button:true},
  {label:"Another-File.md", closeable:true, dirty:true, button:true});

editor_tabs.events.filter(x => x.type === TabEventType.activate ).subscribe(x => {
  console.info(x);
})

editor_tabs.events.filter(x => x.type === TabEventType.rightClick ).subscribe(x => {
  console.info("RC!", x);
})

window['ET'] = editor_tabs;

/**
 * response to a shell command is the next prompt. in some cases, 
 * if the shell enters a (debug) browser, then there is no new 
 * prompt at the end of execution, but we shift back up to the 
 * previous prompt.
 * @param buffer 
 */
let exec_callback = function(buffer:string) : Promise<any>{
  return new Promise((resolve, reject) =>{
    R.ShellExec(buffer).then(result => {
      if( result === -1 ){ resolve({ pop_stack: true }); }
      else resolve({ text: result });
    });
  });
}

let break_callback = function(){
  management_pipe.SendMessage("break");
}

let terminal_config:TerminalConfig = {
  node_: node,
  autocomplete_callback_: autocomplete_callback,
  exec_callback_: exec_callback,
  break_callback_: break_callback,
  formatter_: new RTextFormatter()
};

let terminal = new TerminalImplementation(terminal_config);
terminal.Init();

node.addEventListener("contextmenu", e => {
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
          node.classList.add(className);
        }, delay);
      }
      else {
        if(timeout) clearTimeout(timeout);
        timeout = 0;
        node.classList.remove(className);
      }
    });
  }
}

BusyOverlay.Create(R.busy_status_, node, "busy");

R.console_messages.subscribe((console_message) => {
  if( console_message.type === ConsoleMessageType.PROMPT ){
    console.info(`Prompt (${console_message.id})`, console_message.text);
    terminal.Prompt({
      text: console_message.text,
      push_stack: console_message.id !== 0 // true
    });
  }
  else {
    terminal.PrintConsole(console_message.text, !R.busy);
  }
});

let allow_close = true; // dev // false;

R.control_messages.subscribe(message => {
  console.info( "CM", message );
  if( message === "shutdown" ){
    terminal.CleanUp();
    allow_close = true;
    remote.getCurrentWindow().close();
  } 
});

let Close = function(){
  terminal.CleanUp();
  R.Close().then(() => {
    allow_close = true;
    remote.getCurrentWindow().close();
  }).catch(err => {
    allow_close = true;
    remote.getCurrentWindow().close();
  })
};

// connect/init 

console.info(process);
let pipe_name = process.env['BERT_PIPE_NAME'] || "BERT2-PIPE-X";

setTimeout(() => {
  R.Init({pipe_name}).then( x => {
    window.addEventListener("beforeunload", event => {
      if(allow_close) return;
      event.returnValue = false;
      setImmediate(() => Close());
    });

    management_pipe.Init({pipe_name: pipe_name + "-M"});
    window['mp'] = management_pipe;

  }).catch( e => console.info( "error", e ));
}, 1 );

// deal with splitter change on drag end 

splitter.dragging.filter(x => !x).subscribe(x => {
  terminal.Resize();
  Editor.Resize();
  properties.terminal.split = splitter.split;
});

/////////////////////////////

Editor.Load();

/*
var editor = monaco.editor.create(document.getElementById("container"), {
	value: "// First line\nfunction hello() {\n\talert('Hello world!');\n}\n// Last line",
	language: "javascript",

	lineNumbers: false,
	roundedSelection: false,
	scrollBeyondLastLine: false,
	readOnly: false,
	theme: "vs-dark",
});
setTimeout(function() {
	editor.updateOptions({
		lineNumbers: true
	});
}, 2000);
*/

/*
declare var amdRequire:any;
declare var monaco:any;

  function uriFromPath(_path) {
    var pathName = path.resolve(_path).replace(/\\/g, '/');
    if (pathName.length > 0 && pathName.charAt(0) !== '/') {
      pathName = '/' + pathName;
    }
    return encodeURI('file://' + pathName);
  }
  amdRequire.config({
    baseUrl: uriFromPath(path.join(__dirname, '../node_modules/monaco-editor/min'))
  });
  // workaround monaco-css not understanding the environment
  self['module'] = undefined;
  // workaround monaco-typescript not understanding the environment
  self['process'].browser = true;
  amdRequire(['vs/editor/editor.main'], function() {
    var editor = monaco.editor.create(document.getElementById('editor-container'), {
      value: [
        'function x() {',
        '\tconsole.log("Hello world!");',
        '}'
      ].join('\n'),
      language: 'javascript'
    });
  });
*/

