/// <reference path="../node_modules/monaco-editor/monaco.d.ts" />

import * as path from 'path';

// ambient, declared in html
declare const amd_require:any;

export class Editor {

  private static editor_:monaco.editor.IStandaloneCodeEditor;

  static Load(){

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
  
    amd_require(['vs/editor/editor.main'], function() {
      Editor.editor_ = monaco.editor.create(document.getElementById('editor-container'), {
        language: 'r',
        lineNumbers: "on",
        roundedSelection: true,
        scrollBeyondLastLine: false,
        readOnly: false,
        minimap: { enabled: false }
        // theme: "vs-dark",
      });
  
    });
  
  }

  static Resize(){
    if(Editor.editor_) Editor.editor_.layout();
  }

}

