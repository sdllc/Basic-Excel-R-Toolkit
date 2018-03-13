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

const url = require('url')
const path = require('path');
const electron = require('electron')
// Module to control application life.
const app = electron.app
// Module to create native browser window.
const BrowserWindow = electron.BrowserWindow

const WindowState = require('electron-window-state');

require('electron-reload')(path.join(__dirname,"build"));

let dev_flags = 0;
let pipe_list = [];
let management_pipe = "";

process.env['BERT_CONSOLE_ROOT'] = __dirname;

// we might have (in fact we expect to have) multiple pipes,
// and we need to keep track of all of them. they're not 
// specifically identified, we ask the pipe what language
// it uses.

for( let i = 0; i< process.argv.length; i++ ){

  let arg = process.argv[i];
  let more = (i < (process.argv.length - 1));

  if( arg === "-p" && more ){
    pipe_list.push(process.argv[++i]);
  }
  else if( arg === "-m" && more ){
    management_pipe = process.argv[++i];
  }
  else if( arg === "-d" ){
    dev_flags = Number(process.argv[++i]||1);
    process.env['BERT_DEV_FLAGS'] = dev_flags;
  }
}

if(management_pipe.length){
  process.env['BERT_MANAGEMENT_PIPE'] = management_pipe;
}

if(pipe_list.length){
  process.env['BERT_PIPE_NAME'] = pipe_list.join(";");
}

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let mainWindow

function createWindow () {

  let window_state = WindowState({
    defaultWidth: 1200, defaultHeight: 800
  });

  // Create the browser window.
  mainWindow = new BrowserWindow(window_state);

  window_state.manage(mainWindow);

  // and load the index.html of the app.
  mainWindow.loadURL(url.format({
    pathname: path.join(__dirname, 'index.html'),
    protocol: 'file:',
    slashes: true
  }))

  if(dev_flags) mainWindow.webContents.openDevTools()

  // Emitted when the window is closed.
  mainWindow.on('closed', function () {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow = null
  })
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow)

// Quit when all windows are closed.
app.on('window-all-closed', function () {
  // On OS X it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('activate', function () {
  // On OS X it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (mainWindow === null) {
    createWindow()
  }
})

// In this file you can include the rest of your app's specific main process
// code. You can also put them in separate files and require them here.
