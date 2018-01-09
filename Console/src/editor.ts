/// <reference path="../node_modules/monaco-editor/monaco.d.ts" />

import {remote} from 'electron';
const ElectronDialog = remote.dialog;

import { TabPanel, TabJustify, TabEventType } from './tab-panel';

import * as path from 'path';
import * as fs from 'fs';

import * as Rx from 'rxjs';
import { MenuUtilities } from './menu_utilities';

// ambient, declared in html
declare const amd_require: any;

/**
 * class represents an editor; handles its own layout. basically a wrapper
 * for monaco, adding document handling plus any customization we want to do.
 * 
 * monaco doesn't work as a module, so it requires some html markup and 
 * some extra loading, but once that's done we can treat it as a regular 
 * type (using the reference).
 */
export class Editor {

  /** flag for loading, in case we have multiple instances */
  static loaded_ = false;

  /**
   * finish loading monaco
   */
  static Load() {
    function uriFromPath(_path) {
      var pathName = path.resolve(_path).replace(/\\/g, '/');
      if (pathName.length > 0 && pathName.charAt(0) !== '/') {
        pathName = '/' + pathName;
      }
      return encodeURI('file://' + pathName);
    }
    amd_require.config({
      baseUrl: uriFromPath(path.join(__dirname, '../node_modules/monaco-editor/min'))
    });

    // workaround monaco-css not understanding the environment
    self['module'] = undefined;
    // workaround monaco-typescript not understanding the environment
    self['process'].browser = true;
  }

  /** editor instance */
  private editor_: monaco.editor.IStandaloneCodeEditor;

  /** tab panel */
  private tabs_: TabPanel;

  /** html element containing editor */
  private container_: HTMLElement;

  /** properties */
  private properties_: any;

  /**
   * build layout, do any necessary initialization and then instantiate the
   * editor instance
   * 
   * @param node an element, or a selector we can pass to `querySelector()`.
   */
  constructor(node: string | HTMLElement, properties: any) {

    this.properties_ = properties;

    if (typeof node === "string") this.container_ = document.querySelector(node);
    else this.container_ = node;
    if (!Editor.loaded_) Editor.Load();

    this.container_.classList.add("editor-container");

    let tabs = document.createElement("div");
    tabs.classList.add("editor-tabs");
    this.container_.appendChild(tabs);

    let editor = document.createElement("div");
    editor.classList.add("editor-editor");
    tabs.appendChild(editor);

    this.tabs_ = new TabPanel(tabs);
    this.tabs_.AddTabs(
      { label: "File1.js", closeable: true, button: true },
      { label: "File2.R", closeable: true, button: true },
      { label: "Another-File.md", closeable: true, dirty: true, button: true });

    /*
    editor_tabs.events.filter(x => x.type === TabEventType.activate ).subscribe(x => {
      console.info(x);
    })
    
    editor_tabs.events.filter(x => x.type === TabEventType.rightClick ).subscribe(x => {
      console.info("RC!", x);
    })
    
    editor_tabs.events.filter(x => x.type === TabEventType.buttonClick ).subscribe(x => {
      console.info("button click", x);
    })
    */

    amd_require(['vs/editor/editor.main'], () => {
      this.editor_ = monaco.editor.create(editor, {
        language: 'r',
        lineNumbers: "on",
        roundedSelection: true,
        scrollBeyondLastLine: false,
        readOnly: false,
        minimap: { enabled: false },
        // theme: "vs-dark",
      });

    });

    MenuUtilities.events.subscribe(event => {
      switch(event.id){
      case "main.file.open":
        this.OpenFile();
        break;
      case "main.file.close":
        this.CloseFile();
        break;
      case "main.file.revert":
        this.RevertFile();
        break;
      }
    });

  }

  private OpenFileInternal(file_path:string){
    return new Promise((resolve, reject) => {
      fs.readFile(file_path, "utf8", (err, data) => {
        if(err) return reject(err);

        // create new model...

        // add tab...

        // ... 

        this.editor_.setValue(data); // temp only

      });
    });
  }

  /** 
   * select and open file. if no path is passed, show a file chooser.
   */
  public OpenFile(file_path?:string){
    if(file_path) return this.OpenFileInternal(file_path);
    let files = ElectronDialog.showOpenDialog({
      title: "Open File",
      properties: ["openFile"],
      filters: [
        {name: 'R source files', extensions: ['r', 'rsrc', 'rscript']},
        {name: 'All Files', extensions: ['*']}
      ]
    });
    if( files && files.length ) return this.OpenFileInternal(files[0]);
    return Promise.reject("no file selected");
  }

  public CloseFile(){}
  public RevertFile(){}
  
  /** update layout, should be called after resize */
  public UpdateLayout() {
    if (this.editor_) this.editor_.layout();
  }

}

