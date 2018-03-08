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

/// <reference path="../../node_modules/monaco-editor/monaco.d.ts" />

/**
 * document types with different rendering behavior
 */
export enum DocumentType {
  rendered = "rendered",
  editor = "editor",
  config = "config"
}

/**
 * class represents a document in the editor; has content, view state.
 * extended to support static (i.e. rendered) documents, which are 
 * displayed as html.
 * 
 * FIXME: base type, subtypes for rendered/editor documents?
 */
export class Document {

  /** label: the file name, generally, or "untitled-x" */
  label_: string;

  /** path to file. null for "new" files that have never been saved. */
  file_path_: string;

  /** editor model */
  model_: monaco.editor.IModel;

  /** preserved state on tab switches */
  view_state_: monaco.editor.ICodeEditorViewState;

  /** flag */
  dirty_ = false;

  /** the last saved version, for comparison to AVID, for dirty check */
  saved_version_ = 0; // last saved version ID

  /** local ID */
  id_: number;

  /** rendered: not editable, show rendered content -- md and html? */
  // rendered_ = false;
  type_: DocumentType;

  /** */
  rendered_content_:string;

  /** node for rendered content. not preserved. */
  rendered_content_node_:HTMLElement;

  /** override language (optional) */
  overrideLanguage_: string;

  /** save pending to prevent loop with file watcher; not preserved */
  save_pending_ = false;

  /** 
   * revert pending to prevent side-effects (sigh). it should always be 
   * an indication of bad design if you have to start working around side
   * effects. 
   */
  revert_pending_ = false;

  /** serialize */
  toJSON() {
    return {
      label: this.label_,
      file_path: this.file_path_,
      view_state: this.view_state_,
      dirty: this.dirty_,
      overrideLanguage: this.overrideLanguage_,
      uri: this.model_ ? this.model_.uri : null,
      type: this.type_,
      saved_version: this.saved_version_,
      alternative_version_id: this.model_ ? this.model_.getAlternativeVersionId() : 0,
      text: this.model_ ? this.model_.getValue() : this.rendered_content_
    }
  }

}
