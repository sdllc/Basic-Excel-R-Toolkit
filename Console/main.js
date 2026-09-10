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
const path = require('path');
const { app, BrowserWindow } = require('electron');
const remote = require('@electron/remote/main');
const WindowState = require('electron-window-state');

remote.initialize();

// for automated testing: expose the devtools protocol on this port, so a
// test can drive the console and read its state without touching the UI
if (process.env.BERT_CONSOLE_DEBUG_PORT) {
  app.commandLine.appendSwitch('remote-debugging-port', process.env.BERT_CONSOLE_DEBUG_PORT);
}

let dev_flags = 0;
let pipe_list = [];
let management_pipe = "";

process.env['BERT_CONSOLE_ROOT'] = __dirname;

// command line: -m <management pipe>, -p <language pipe> (repeated), -d <dev flags>

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

let mainWindow;

function createWindow () {

  let window_state = WindowState({
    defaultWidth: 1200, defaultHeight: 800
  });

  // the renderer talks to the add-in over named pipes and reads and watches
  // files itself, so it runs with node available. that is the architecture
  // the console has always had; current electron just makes it an explicit
  // choice instead of the default.

  mainWindow = new BrowserWindow({
    x: window_state.x,
    y: window_state.y,
    width: window_state.width,
    height: window_state.height,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false
    }
  });

  remote.enable(mainWindow.webContents);
  window_state.manage(mainWindow);

  mainWindow.loadFile(path.join(__dirname, 'index.html'));

  if(dev_flags) mainWindow.webContents.openDevTools();

  mainWindow.on('closed', function () {
    mainWindow = null;
  });

}

app.whenReady().then(createWindow);

app.on('window-all-closed', function () {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', function () {
  if (mainWindow === null) {
    createWindow();
  }
});
