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

  // editor tab context menu

  EditorTab: [
    { label: 'Close', id: 'editor-tab-close' },
    { label: 'Close Others', id: 'editor-tab-close-others' },
    { label: 'Close All', id: 'editor-tab-close-all' }
  ],

  // editor body context menu

  EditorContext: [
    { label: 'Select All', id: 'editor-select-all' },
    { role: 'cut' },
    { role: 'copy' },
    { role: 'paste' },
    { type: 'separator' },
    {
      label: "Execute selected code (F6)",
      id: 'editor-execute',
      enabled: false
    }
  ],

  // shell context menu

  ShellContext: [
    { label: 'Select All', id: 'shell-select-all' },
    { role: 'copy' },
    { role: 'paste' },
    { type: 'separator' },
    { label: 'Clear Shell', id: 'shell-clear-shell' }
  ],

  // main menu (menu bar)

  Main: [
    {
      label: "File",
      submenu: [
        {
          id: "file-new",
          label: "New",
          accelerator: "CmdOrCtrl+N"
        },
        {
          id: "file-open",
          label: "Open...",
          accelerator: "CmdOrCtrl+O"
        },
        {
          id: "open-recent",
          submenu: [],
          label: "Open Recent"
        },
        {
          id: "file-save",
          label: "Save",
          accelerator: "CmdOrCtrl+S"
        },
        {
          id: "file-save-as",
          label: "Save As...",
          accelerator: "Ctrl+Shift+S"
        },
        {
          id: "file-revert",
          label: "Revert"
        },
        {
          id: "file-close",
          label: "Close Document",
          accelerator: "CmdOrCtrl+W"
        },
        {
          type: "separator"
        },
        {
          role: "quit",
          label: "Close BERT Console"
        }
      ]
    },
    {
      label: "Edit",
      submenu: [
        {
          role: "undo"
        },
        {
          role: "redo"
        },
        {
          type: "separator"
        },
        {
          role: "cut"
        },
        {
          role: "copy"
        },
        {
          role: "paste"
        },
        {
          role: "delete"
        },
        {
          role: "selectall"
        },
        {
          type: "separator"
        },
        {
          id: "find",
          label: "Find",
          accelerator: "Ctrl+F"
        },
        {
          id: "replace",
          label: "Replace",
          accelerator: "Ctrl+H"
        }
      ]
    },
    {
      label: "View",
      submenu: [
        {
          label: "Show Editor",
          type: "checkbox",
          setting: "editor.hide",
          invert: true,
          accelerator: "Ctrl+Shift+E"
        },
        {
          label: "Show R Shell",
          type: "checkbox",
          setting: "shell.hide",
          invert: true,
          accelerator: "Ctrl+Shift+R"
        },
        {
          label: "Layout",
          submenu: [
            {
              label: "Top and Bottom",
              type: "radio",
              setting: "layout.vertical",
              invert: false
            },
            {
              label: "Side by Side",
              type: "radio",
              setting: "layout.vertical",
              invert: true
            }
          ]
        },
        {
          type: "separator"
        },
        {
          label: "Editor",
          submenu: [
            {
              id: "editor-theme",
              label: "Theme"
            },
            {
              label: "Show Line Numbers",
              type: "checkbox",
              setting: "editor.CodeMirror.lineNumbers"
            },
            {
              label: "Show Status Bar",
              type: "checkbox",
              setting: "editor.statusBar"
            }
          ]
        },
        {
          label: "Shell",
          submenu: [
            {
              id: "shell-theme",
              label: "Theme"
            },
            {
              label: "Show Function Tips",
              type: "checkbox",
              setting: "shell.functionTips"
            },
            {
              label: "Update Console Width on Resize",
              type: "checkbox",
              setting: "shell.resize"
            },
            {
              label: "Wrap Long Lines",
              type: "checkbox",
              setting: "shell.wrap"
            }
          ]
        },
        {
          id: "user-stylesheet",
          label: "User Stylesheet"
        },
        "separator",
        {
          label: "Developer",
          submenu: [
            {
              label: "Allow Reloading",
              type: "checkbox",
              setting: "developer.allowReloading"
            },
            {
              id: "reload",
              label: "Reload",
              accelerator: "CmdOrCtrl+R"
            },
            {
              label: "Toggle Developer Tools",
              accelerator: "Ctrl+Shift+I",
              id: "toggle-developer"
            }
          ]
        }
      ]
    },
    {
      label: "Packages",
      submenu: [
        {
          id: "r-packages-choose-mirror",
          label: "Choose CRAN Mirror"
        },
        {
          id: "r-packages-install-packages",
          label: "Install Packages"
        }
      ]
    },
    {
      label: "Help",
      submenu: [
        {
          id: "help-learn-more",
          label: "Learn More"
        },
        {
          id: "help-feedback",
          label: "Feedback"
        },
        {
          id: "help-issues",
          label: "Issues"
        },
        {
          type: "separator"
        },
        {
          enabled: false,
          id: "bert-shell-version",
          label: "BERT Shell Version"
        }
      ]
    }
  ]
};

