
import {remote} from 'electron';
const {Menu, MenuItem} = remote;

import * as Rx from 'rxjs';

import * as path from 'path';
import * as fs from 'fs';

interface LocalMenuItem {
  items?:LocalMenuItem[];
  label?:string;
  id?:string;
  checked?:boolean;
  accelerator?:string;
  type?:"normal"|"separator"|"submenu"|"checkbox"|"radio";
}

export interface MenuEvent {
  id?:string
}

/**
 * class abstracts menu installation, handling
 */
export class MenuUtilities {

  private static events_:Rx.Subject<MenuEvent> = new Rx.Subject<MenuEvent>();

  private static menus_:any = {};

  public static get events() { return this.events_; }

  /** 
   * find the item, by id (path). 
   * 
   * this should only be used by this class. don't let menu items leak into 
   * other classes, we want to abstract all menu behavior.
   */
  private static Find(id:string):Electron.MenuItem {

    let elements = id.split(".");
    if(!elements.length) throw( "invalid menu id");

    let item_path = elements.shift();
    let menu:Electron.Menu = this.menus_[item_path];
    if(!menu) throw( "invalid menu root" );

    let menu_item:Electron.MenuItem;

    elements.forEach(element => {
      item_path += "." + element;
      menu_item = menu.items.find(item => (item['id'] === item_path));
      if(!menu_item) throw( "item not found: " + item_path);
      menu = menu_item['submenu'];
    });

    return menu_item;
  }  

  /** get check from menu item, by path */
  static GetCheck(id:string){
    let menu_item = this.Find(id);
    return menu_item.checked;
  }

  /** set menu item check and optionally trigger the related event */
  static SetCheck(id:string, checked=true, trigger_event=false){
    let menu_item = this.Find(id);
    menu_item.checked = checked;
    if(trigger_event && menu_item.click) menu_item.click.call(0);
  }

  private static Click(id:string, menuItem:Electron.MenuItem, browserWindow, event){
    this.events_.next({id});
  }

  static Install(template){

    let build_menu = (items, id) => {
      let menu = new Menu();
      items.map(item => {
        let options:any = {
          label: item.label,
          type: item.type,
          checked: item.checked,
          accelerator: item.accelerator,
          id: id + "." + (item.id||item.label)
        };
        if( item.items && item.items.length ) options.submenu = build_menu(item.items, options.id);
        else options.click = this.Click.bind(this, options.id);
        menu.append(new MenuItem(options));
      });
      return menu;
    }

    let items = template.items as LocalMenuItem[];
    let main_menu = build_menu(items, "main");
    this.menus_.main = main_menu;

    let disabled_menu = new Menu();
    main_menu.items.forEach(item => {
      let options = {
        label: item.label,
        enabled: false
      }
      disabled_menu.append(new MenuItem(options));
    });
    this.menus_.disabled = disabled_menu;

    this.Enable();
  }

  /** disable main application menu */
  static Disable(){
    Menu.setApplicationMenu(this.menus_.disabled);
  }

  /** enable main application menu */
  static Enable(){
    Menu.setApplicationMenu(this.menus_.main);
  }

  static Load(menu_data_path){
    return new Promise((resolve, reject) => {
      fs.readFile( menu_data_path, "utf8", (err, data) => {
        if(err) return reject(err);
        try {
          let template = JSON.parse(data);
          if( template['main-menu'] ) this.Install(template['main-menu']);
        }
        catch(e){
          return reject(e);
        }
        resolve();
      })
    });
  }
  
}
