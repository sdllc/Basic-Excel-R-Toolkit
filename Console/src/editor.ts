/// <reference path="../node_modules/monaco-editor/monaco.d.ts" />

import {remote} from 'electron';

import { MenuUtilities } from './menu_utilities';
import * as JuliaLanguage from './julia-language';
import { TabPanel, TabJustify, TabEventType, TabSpec } from './tab-panel';

import * as path from 'path';
import * as fs from 'fs';
import * as Rx from 'rxjs';

// ambient, declared in html
declare const amd_require: any;

/**
 * class represents a document in the editor; has content, view state
 */
class Document {
  model_:monaco.editor.IModel;
  view_state_:monaco.editor.ICodeEditorViewState;
  dirty_ = false;
}

class EditorStatusBar {

  private node_:HTMLElement;
  private label_:HTMLElement;
  private position_:HTMLElement;
  private language_:HTMLElement;
  
  public get node() { return this.node_; }

  public set label(text){ this.label_.textContent = text; }
  
  public set language(text){ this.language_.textContent = text; }

  public set position([line, column]){ 
    this.position_.textContent = `Line ${line}, Col ${column}`;
  }

  constructor(){

    this.node_ = document.createElement("div");
    this.node_.classList.add("editor-status-bar");

    this.label_ = document.createElement("div");
    this.node_.appendChild(this.label_);

    let spacer = document.createElement("div");
    spacer.classList.add("spacer");
    this.node_.appendChild(spacer);

    this.position_ = document.createElement("div");
    this.node_.appendChild(this.position_);

    this.language_ = document.createElement("div");
    this.node_.appendChild(this.language_);

  }

}

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

  /** utility function */
  static UriFromPath(_path) {
    var pathName = path.resolve(_path).replace(/\\/g, '/');
    if (pathName.length > 0 && pathName.charAt(0) !== '/') {
      pathName = '/' + pathName;
    }
    return encodeURI('file://' + pathName);
  }

  /**
   * finish loading monaco
   */
  static Load() : Promise<void> {

    if(this.loaded_) return Promise.resolve();
    return new Promise((resolve, reject) => {

      this.loaded_ = true;

      amd_require.config({
        baseUrl: Editor.UriFromPath(path.join(__dirname, '../node_modules/monaco-editor/min'))
      });

      // workaround monaco-css not understanding the environment
      self['module'] = undefined;
      // workaround monaco-typescript not understanding the environment
      self['process'].browser = true;

      // this is async
      amd_require(['vs/editor/editor.main'], () => {

        // register additional languages (if any)
        // TODO: abstract this

        let conf = { id: 'julia', extensions: ['.jl', '.julia'] };
        monaco.languages.register(conf);    
        monaco.languages.onLanguage(conf.id, () => {
          monaco.languages.setMonarchTokensProvider(conf.id, JuliaLanguage.language);
          monaco.languages.setLanguageConfiguration(conf.id, JuliaLanguage.conf);
        });

        resolve();

      });
    });

  }

  private status_bar_ = new EditorStatusBar();

  /** editor instance */
  private editor_: monaco.editor.IStandaloneCodeEditor;

  /** tab panel */
  private tabs_: TabPanel;

  /** html element containing editor */
  private container_: HTMLElement;

  /** properties */
  private properties_: any;

  private documents_:Document[] = [];

  /**
   * builds layout, does any necessary initialization and then instantiates 
   * the editor instance
   * 
   * @param node an element, or a selector we can pass to `querySelector()`.
   */
  constructor(node: string | HTMLElement, properties: any) {

    this.properties_ = properties;

    if (typeof node === "string") this.container_ = document.querySelector(node);
    else this.container_ = node;

    this.container_.classList.add("editor-container");

    let tabs = document.createElement("div");
    tabs.classList.add("editor-tabs");
    this.container_.appendChild(tabs);

    let editor = document.createElement("div");
    editor.classList.add("editor-editor");
    tabs.appendChild(editor);

    this.container_.appendChild(this.status_bar_.node);
    this.status_bar_.label = "Ready";

    this.tabs_ = new TabPanel(tabs);
   
    this.tabs_.events.filter(event => event.type === TabEventType.deactivate ).subscribe(event => {
      this.DeactivateTab(event.tab);
    });
    
    this.tabs_.events.filter(event => event.type === TabEventType.activate ).subscribe(event => {
      this.ActivateTab(event.tab);
    });

    /*
    editor_tabs.events.filter(x => x.type === TabEventType.rightClick ).subscribe(x => {
      console.info("RC!", x);
    })
    
    editor_tabs.events.filter(x => x.type === TabEventType.buttonClick ).subscribe(x => {
      console.info("button click", x);
    })
    */

    // the load call ensures monaco is loaded via the amd loader;
    // if it's already been loaded once this will return immediately
    // via Promise.resolve().

    // FIXME: options

    Editor.Load().then(() => {
      this.editor_ = monaco.editor.create(editor, {
        model: null, // don't create an empty model
        lineNumbers: "on",
        roundedSelection: true,
        scrollBeyondLastLine: false,
        readOnly: false,
        minimap: { enabled: false },
        // theme: "vs-dark",
      });

      // cursor position -> status bar
      this.editor_.onDidChangeCursorPosition(event => {
        this.status_bar_.position = [event.position.lineNumber, event.position.column];
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

  /** deactivates tab and saves view state. */
  private DeactivateTab(tab:TabSpec){
    if( tab.data ){
      let document = tab.data as Document;
      document.view_state_ = this.editor_.saveViewState();
      console.info("VS", document.view_state_);
    }
  }

  /** activates tab, loads document and restores view state (if available) */
  private ActivateTab(tab:TabSpec){
    if( tab.data ){
      let document = tab.data as Document;
      if( document.model_ ) {
        this.editor_.setModel(document.model_);
        if( document.view_state_) 
          this.editor_.restoreViewState(document.view_state_);
        let language = document.model_['_languageIdentifier'].language;
        language = language.substr(0,1).toUpperCase() + language.substr(1);
        this.status_bar_.language = language;
      }
    }
  }

  /** load a file from a given path */
  private OpenFileInternal(file_path:string){

    // FIXME: already open? switch to (...)

    return new Promise((resolve, reject) => {
      fs.readFile(file_path, "utf8", (err, data) => {
        if(err) return reject(err);

        let document = new Document();

        document.model_ = monaco.editor.createModel(data, undefined, 
          monaco.Uri.parse(Editor.UriFromPath(file_path)));

        let tab:TabSpec = {
          label: path.basename(file_path),
          tooltip: file_path,
          closeable: true,
          button: true,
          dirty: false,
          data: document
        };

        this.tabs_.AddTabs(tab);
        this.tabs_.ActivateTab(tab);

      });
    });
  }

  /** 
   * select and open file. if no path is passed, show a file chooser.
   */
  public OpenFile(file_path?:string){
    if(file_path) return this.OpenFileInternal(file_path);
    let files = remote.dialog.showOpenDialog({
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

