/**
 * Copyright (c) 2016-2017 Structured Data, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to 
 * deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

"use strict";

module.exports = {

  Main: {

    // status notifications

    EDITOR: "Editor",
    SHELL: "Shell",
    CHANGE_FOCUS: "$1 has focus",
    CHANGE_FOCUS_LONG: "$1 has focus (use Ctrl+E to switch)",
    SPLITTER_DRAG: "Layout: $1% / $2%",
    
    INVALID_SETTINGS_FILE: "Invalid settings file",

    // dialog titles

    ERROR: "ERROR",
    WARNING: "WARNING",
    
    // dialog messages and buttons

    READING_FILE: "reading file $1",
    WRITING_FILE: "writing file $1",
    NOT_CONNECTED: "Not connected to Excel/R",

    // specific to the update available message

    UPDATE_AVAILABLE: "Version $1 is available",
    DOWNLOAD: "Download update",
    IGNORE: "Don't notify me again",

    // general buttons

    OK: "OK",
    CANCEL: "Cancel",

    // downloading 

    TRYING_URL: "Trying URL",
    DOWNLOADING: "Downloading",
    DOWNLOAD_COMPLETE: "Download Complete",
    DOWNLOAD_FAILED: "Download Failed",

    // packages 

    INSTALLED: "installed",
    PACKAGE_SELECTED_SINGLE: "Package selected",
    PACKAGE_SELECTED_PLURAL: "Packages selected",

    FILTER: "Filter...",
    LOADING_PACKAGE_LIST: "Loading package list, please wait...",
    AVAILABLE_PACKAGES: "Available Packages",

    // mirrors

    SELECT_MIRROR: "Select a CRAN Mirror",
    LOADING_MIRROR_LIST: "Loading mirror list, please wait...",

  },

  Editor: {

    UNTITLED: "Untitled",

    // status info

    LINE_COL: "Line $1, Col $2",
    LANGUAGE: "Language: $1",

    // when you click close before saving

    QUERY_SAVE: "Save changes to $1?",
    SAVE: "Save",
    DONT_SAVE: "Don't Save",
    CANCEL: "Cancel",

    // file dialog

    R_FILES_PATTERN: 'R Files',
    ALL_FILES_PATTERN: 'All Files',

    // for the editor html template (mostly search)

    FIND: "Find",
    CASE_SENSITIVE: "Case Sensitive",
    WHOLE_WORD: "Whole Word",
    REGEX: "Regex",
    FIND_PREVIOUS: "Find Previous",
    FIND_NEXT: "Find Next",
    CLOSE_SEARCH_PANEL: 'Close Search Panel',
    REPLACE: "Replace",
    REPLACE_ONE: "Replace One",
    REPLACE_ALL: "Replace All",

  }

};

