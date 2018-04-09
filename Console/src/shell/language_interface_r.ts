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

import { LanguageInterface } from './language_interface';
import { RTextFormatter } from './text_formatter';
import { Pipe } from '../io/pipe';
import { Pipe2 } from '../io/management_pipe';

import { GraphicsDevice } from './graphics_device';
import { SVGGraphicsDevice } from './svg_graphics_device';
import { PNGGraphicsDevice } from './png_graphics_device';
import { TerminalImplementation } from './terminal_implementation';

import { MenuUtilities } from '../ui/menu_utilities';
import { Dialog, DialogSpec, DialogButton } from '../ui/dialog';
import { DataCache } from '../common/data_cache';
import { VirtualList } from '../ui/virtual_list';

import * as path from 'path';
const Constants = require("../../data/constants.json");

// FIXME: move to different file, language-specific
const ApplicationMenu = require( "../../data/menus/application_menu.json");

/** specialization: R */
export class RInterface extends LanguageInterface {

  static language_name_ = "R";
  static target_version_ = [ 3, 4, 4 ];

  formatter_ = new RTextFormatter();

  static UpdateMenus(){

    let menu_items = ApplicationMenu['r-packages'];
    MenuUtilities.AppendMenu("main.packages", menu_items);

  }

  constructor(){
    super();
    RInterface.UpdateMenus();
    MenuUtilities.events.subscribe(event => {
      switch(event.id){
      case "main.packages.r-choose-mirror":
        this.ChooseMirror();
        break; 
      case "main.packages.r-install-packages":
        this.SelectPackages();
        break; 
      }
    });
  }

  InitPipe(pipe:Pipe, name:string){
    super.InitPipe(pipe, name);
    this.management_pipe_ = new Pipe2();
    this.management_pipe_.Init({pipe_name: name + "-M"});
  }

  AttachTerminal(terminal:TerminalImplementation){

    // FIXME: this is language-specific, so it should be in the 
    // language implementation class

    let svg_graphics = new SVGGraphicsDevice();
    let png_graphics = new PNGGraphicsDevice();

    // FIXME: (1) have someone else dispatch. (2) "activate" a device
    // so we don't have to switch on every packet.

    // NOTE: re: (2), if we have to check for a particular message type,
    // then we have to check that in every packet, and so we're testing
    // on every packet anyway.

    this.pipe_.graphics_message_handler = (message) => {
      let command = message.toObject().graphics;
      switch(command.deviceType){
      case "svg":
        return svg_graphics.GraphicsCommand(message, command);
      default:
        return png_graphics.GraphicsCommand(message, command);
      }
    }

    GraphicsDevice.new_graphic.subscribe(x => {
      terminal.InsertGraphic(x.height, x.element);
    });

    this.subscribe(x => {
      if(x.type === "paste") terminal.Paste(x.data);
    })

  }

  async AutocompleteCallback(buffer:string, position:number) {
    buffer = buffer.replace( /\\/g, '\\\\').replace( /"/g, '\\"' );
    let result = await this.pipe_.Internal(`BERT$.Autocomplete("${buffer}",${position})`);
    return result;
  }

  async ExecCallback(buffer:string){
    let result = await this.pipe_.ShellExec(buffer);
    if( result === -1 ) return { pop_stack: true };
    return { text:result };
  }

  BreakCallback(){
    this.management_pipe_.SendMessage("break");
  }
 
  /** 
   * shows the "select packages" dialog.
   */
  async SelectPackages(){
    
    // ensure we have a repo

    let repos = await this.pipe_.Internal(`getOption("repos")`);
    if(!repos || !repos.CRAN || !/^http/.test(repos.CRAN)){
      let success = await this.ChooseMirror();
      if(!success) return;
    }

    repos = await this.pipe_.Internal(`getOption("repos")`);
    
    // ok, show a dialog and then get the list

    let key = "cran-package-list";
    let cacheResult = DataCache.Get(key);
    let data;

    let spec:DialogSpec = {
      title: Constants.dialogs.selectPackages.selectPackagesTitle, 
      escape: true,
      className: "cran-package-chooser",
      buttons: [Constants.dialogs.buttons.cancel]
    }

    spec.body = document.createElement("div");
    spec.body.className = "list-container";

    let header = document.createElement("div");
    header.className = "list-header";
    spec.body.appendChild(header);

    let header_container = document.createElement('div');
    header_container.className = "list-header-layout";
    header.appendChild(header_container);

    let header_text = document.createElement("div");
    header_text.textContent = Constants.dialogs.selectPackages.pleaseWait;
    header_container.appendChild(header_text);

    let dialog_result = Dialog.Show(spec);

    if(cacheResult.status !== DataCache.CacheStatus.Valid){

      // NOTE: we're not using the package list. use the html list.

      let html_list = await fetch(path.join(repos.CRAN, "web/packages/available_packages_by_name.html"));
      let html_blob = await html_list.blob();
      let reader = new FileReader();

      await new Promise((resolve, reject) => {
        reader.onloadend = () => {
          resolve();
        }
        reader.onerror = () => {
          reject();
        }
        reader.readAsText(html_blob);
      });

      let html = reader.result;
      let regex = /<tr>[\s\S]*?<td>[\s\S]*?<a.*?>(.*?)<\/a>[\s\S]*?<\/td>[\s\S]*?<td>([\s\S]*?)<\/td>/g;
      let match;
      
      data = [];
      let index = 0;
      while(match = regex.exec(html)){
        data.push({name: match[1], description: match[2], index:index++});
      }
      DataCache.Store(key, data);

      reader.onerror = null;
      reader.onloadend = null;

    }
    else {
      data = cacheResult.data;
    }

    // in case user has pressed cancel while waiting
    if( dialog_result.fulfilled ) return false;
    
    header_text.textContent = Constants.dialogs.selectPackages.pleaseWaitInstalled;

    // get installed packages so we can indicate 
    let installed_packages = await this.pipe_.Internal(`installed.packages()`);
    
    // in case user has pressed cancel while waiting
    if( dialog_result.fulfilled ) return false;

    // this is going to be expensive [actually not too bad step-wise]
    // it runs the full loop on internal packages. we should white-list.
    // UPDATE: remove items with Priority=base; those are defaults. there's
    // one more for translations that we can (probably) safely remove.
    
    let package_index = 0;
    let installed = installed_packages.data.Package.filter((p, index) => {
      return (installed_packages.data.Priority[index] !== "base") && 
        (p !== "translations");
    });

    for( let i = 0; i< installed.length; i++){
      let j, compare = installed[i];
      for( j = package_index; j< data.length; j++ ){
        if( data[j].name === compare ){
          data[j].installed = true;
          data[j].name = data[j].name + ` (${Constants.dialogs.selectPackages.installed})`;
          package_index = ++j;
          break;
        }
      }
      if( j === data.length ) console.info("missing?", compare);
    }
    
    // update dialog
    header_text.textContent = Constants.dialogs.selectPackages.selectPackages;

    // filter should dynamically update the list
    let filter = document.createElement("input");
    filter.type = "text";
    filter.placeholder = Constants.dialogs.selectPackages.filter;
    filter.classList.add("package-filter");
    header_container.appendChild(filter);

    let node = document.createElement("div");
    node.className = "list-body cran-package-list";
    spec.body.appendChild(node);

    spec.enter = true;
    spec.buttons = [Constants.dialogs.buttons.cancel, {
      text:Constants.dialogs.buttons.install, default:true}];
    
    Dialog.Update(spec);

    let virtual_list = new VirtualList();
    let last_click = -1;

    let selected_count = 0;

    // click to select or deselect, support shift-click to batch select
    let click = (x, e) => {
      let entry = data[x.index];
      if(!entry.installed) entry.selected = !entry.selected;
      selected_count += (entry.selected ? 1 : -1);

      if(e.shiftKey && last_click >= 0){
        let start = Math.min(last_click, x.index);
        let end = Math.max(last_click, x.index);
        for( ; start <= end; start++ ){
          if(!data[start].installed && data[start].selected !== entry.selected){
            data[start].selected = entry.selected;
            selected_count += (entry.selected ? 1 : -1);
          }
        }
      }
      virtual_list.Repaint();
      last_click = x.index;

      // report count in status bar
      let status;
      if(selected_count === 1) {
        status = (Constants.dialogs.selectPackages.singularSelected||"").replace(/#/, 1);
      }
      else if(selected_count > 1){
        status = (Constants.dialogs.selectPackages.pluralSelected||"").replace(/#/, selected_count);
      }
      else status = "";
      spec.status = status;
      Dialog.Append({status});
    }

    // format one node
    let format = (node:any, array, index, width = 0) => {

      if(!node.initialized){

        node.initialized = true;
        if(width) node.style.width = width + "px";

        let package_name = document.createElement("div");
        package_name.className = "list-entry-name";
        node.appendChild(package_name);
        node.package_name = package_name;

        let package_description = document.createElement("div");
        package_description.className = "list-entry-description";
        node.appendChild(package_description);
        node.package_description = package_description;

      }

      let data = array[index];
      let className = "";
      
      if(data.installed) className = "installed";
      else if(data.selected) className = "selected";

      node.className = "list-body-entry " + className;
      node.index = data.index;
      node.package_name.innerText = data.name;
      node.package_description.innerHTML = data.description;

    }

    // what is this about: for a fixed height list, we only measure
    // one node. we want to measure the longest-width one, so width
    // is padded out appropriately, without requiring measuring all 
    // of them. we can give the list a hint telling it to measure a
    // specific node.

    let longest_hint = 0;
    let longest_length = 0;

    for( let i = 0; i< data.length; i++ ){
      let s = data[i].name + data[i].description;
      if(s.length > longest_length){
        longest_length = s.length;
        longest_hint = i;
      }
    }

    let options = {
      click,
      format, 
      data, 
      data_length:data.length, 
      outer_node:node, 
      containing_class_name: "cran-package-list",
      fixed_height:true,
      hint:longest_hint
    };

    let inner_node = virtual_list.CreateList(options);
    node.appendChild(inner_node);

    let last_filter = "";
    let update_filter = e => {

      if(filter.value.trim() === last_filter) return;
      last_filter = filter.value.trim();

      if(last_filter === "" ){
        options.data = data;
        options.data_length = data.length;
      }
      else {
        let rex = new RegExp(filter.value.trim(), "i");
        let filtered = data.filter(x => {
          return rex.test(x.name + " " + x.description);
        })
        options.data = filtered;
        options.data_length = filtered.length;
      }

      virtual_list.Update(options);

    };

    filter.addEventListener("input", update_filter);

    let result = await dialog_result; // nice
    let success = false;

    if( result.reason === "enter_key" ||
        (result.reason === "button" && result.data === Constants.dialogs.buttons.install)){
      if(selected_count >= 0){
        let selected = data.filter(x => x.selected).map(x => `"${x.name}"`);
        console.info("install", selected_count, selected);
        let command = `install.packages(c(${selected.join(", ")}))\n`;
        this.next({type: "paste", data:command});
        success = true;
      }
    }
    virtual_list.CleanUp();
    filter.removeEventListener("input", update_filter);
    node.removeChild(inner_node); // _should_ dump listeners. fingers crossed.

    return success;
    
  }

  /**
   * shows the "choose mirror" dialog. on a successful result sets the 
   * mirror in the session (via `options`).  only shows https mirrors;
   * we could offer an option, but not atm.
   * 
   * TODO: put mirror in config (prefs)
   * TODO: have controlr load mirror from config
   * 
   * FIXME: mirror change should flush cached package list 
   * 
   */
  async ChooseMirror(){

    let key = "cran-mirror-list";
    let cacheResult = DataCache.Get(key);
    let data;

    let spec:DialogSpec = {
      title: Constants.dialogs.selectMirror.chooseMirrorTitle, 
      escape: true,
      className: "cran-mirror-chooser",
      buttons: [Constants.dialogs.buttons.cancel]
    }

    spec.body = document.createElement("div");
    spec.body.className = "list-container";

    let header = document.createElement("div");
    header.className = "list-header";
    header.textContent = Constants.dialogs.selectMirror.pleaseWait;
    spec.body.appendChild(header);

    let dialog_result = Dialog.Show(spec);

    if(cacheResult.status !== DataCache.CacheStatus.Valid){
      data = await this.pipe_.Internal(`getCRANmirrors()`);
      DataCache.Store(key, data);
    }
    else {
      data = cacheResult.data;
    }

    let repos = await this.pipe_.Internal(`getOption("repos")`);

    // in case user has pressed cancel while waiting

    if( dialog_result.fulfilled ) return false;

    // filter data

    let indexes = [];
    data.data.URL.forEach((url, index) => { if( /^https/.test(url)) indexes.push(index); });

    let selected_index = -1;
    let first_item = 0;
    let repo = (repos && repos.CRAN && /^http/i.test(repos.CRAN)) ? repos.CRAN : null;

    let filtered = indexes.map((index, n) => {
      let url = data.data.URL[index];
      if( url === repo || url === repo + "/"){
        selected_index = index;
        first_item = n;
      }
      return {
        index, name: data.data.Name[index], host: data.data.Host[index]
      }
    });

    header.textContent = Constants.dialogs.selectMirror.selectMirror;

    let node = document.createElement("div");
    node.className = "list-body cran-mirror-list";
    spec.body.appendChild(node);

    spec.enter = true;
    spec.buttons = [Constants.dialogs.buttons.cancel, {
      text:Constants.dialogs.buttons.select, default:true}];
    
    Dialog.Update(spec);

    let virtual_list = new VirtualList();

    let click = (x, e) => {
      selected_index = x.index;
      virtual_list.Repaint();
    }

    let format = (node:any, array, index, width = 0) => {

      if(!node.initialized){

        node.initialized = true;
        if(width) node.style.width = width + "px";
  
        let entry_header = document.createElement("div");
        entry_header.className = "list-entry-header";
        node.appendChild(entry_header);
        node.header = entry_header;
  
        let entry_subheader = document.createElement("div");
        entry_subheader.className = "list-entry-subheader";
        node.appendChild(entry_subheader);
        node.subheader = entry_subheader;
  
      }

      let data = array[index];
      node.className = "list-body-entry" + (data.index === selected_index ? " selected": "");
      node.index = data.index;
      node.header.innerText = data.name;
      node.subheader.innerText = data.host;
    }

    let inner_node = virtual_list.CreateList({
      click,
      format, 
      data:filtered, 
      data_length:indexes.length, 
      outer_node:node, 
      containing_class_name: "cran-mirror-list"
    });

    node.appendChild(inner_node);
    virtual_list.ScrollIntoView(first_item);
    
    let result = await dialog_result; // nice
    let success = false;

    if( result.reason === "enter_key" ||
        (result.reason === "button" && result.data === Constants.dialogs.buttons.select)){
      if(selected_index >= 0){
        let url = data.data.URL[selected_index];
        let name = data.data.Name[selected_index];

        // FIXME: put this on the command line, then exec normally; 
        // this might take a while.

        this.pipe_.Internal(`options(repos=c(CRAN="${url}", CRANextra="${repos.CRANextra}")); cat("${(Constants.dialogs.selectMirror.setRepo||"").replace(/#/, name)}\n")`);
        success = true;
      }
    }
    virtual_list.CleanUp();
    node.removeChild(inner_node); // _should_ dump listeners. fingers crossed.

    return success;
  }

}