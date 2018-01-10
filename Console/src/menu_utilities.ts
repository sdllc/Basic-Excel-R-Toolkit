
import {remote} from 'electron';
const {Menu, MenuItem} = remote;

import * as Rx from 'rxjs';
import * as path from 'path';
import * as fs from 'fs';

export interface LocalMenuItem {

  // submenu
  items?:LocalMenuItem[];

  // displayed text
  label?:string;

  // id for path in events
  id?:string;

  // enable/disable; default is ENABLE
  enabled?:boolean;

  // checked/unchecked; default false
  checked?:boolean;

  // key accelerator
  accelerator?:string;

  // arbitrary data
  data?:any;

  // defaults to "normal"
  type?:"normal"|"separator"|"submenu"|"checkbox"|"radio";

}

export interface MenuEvent {
  id?:string;
  item?:LocalMenuItem;
}

/**
 * class abstracts menu installation, handling
 */
export class MenuUtilities {

  private static events_:Rx.Subject<MenuEvent> = new Rx.Subject<MenuEvent>();

  private static loaded_:Rx.BehaviorSubject<boolean> = new Rx.BehaviorSubject(false);

  public static get loaded() { return this.loaded_; }

  private static menus_:any = {};

  private static template_:any;

  public static get events() { return this.events_; }

  private static Find(id:string) : LocalMenuItem {

    let elements = id.split(".");
    if(!elements.length) throw( "invalid menu id");
    
    let item_root = elements.shift(); 
    
    let item = this.template_;
    if(!item) throw( "missing root");

    elements.forEach(element => {
      item = item.items.find(item => item.id === element);
      if(!item) throw( "Couldn't find item: " + element );
    });

    return item;

  }

  /** get check from menu item, by path */
  static GetCheck(id:string){
    let item = this.Find(id);
    return !!item.checked;

  }

  /** set menu item check and optionally trigger the related event */
  static SetCheck(id:string, checked=true){ // , trigger_event=false){
    let item = this.Find(id);
    if(item.checked !== checked){
      item.checked = checked;
      this.Update();
    }
  }

  /** enable/disable */
  static SetEnabled(id:string, enabled=true){
    let item = this.Find(id);
    if( item.enabled !== enabled ){
      item.enabled = enabled;
      this.Update();
    }
  }

  /** update submenu. for recent files, essentially. */
  static SetSubmenu(id:string, items:LocalMenuItem[]){
    let item = this.Find(id);
    item.items = items;
    item.enabled = (items.length > 0);
    this.Update();

  }

  private static Click(id:string, item:LocalMenuItem, menuItem:Electron.MenuItem, browserWindow, event){

    // special: handle checks, map back to item
    if((typeof item.checked !== "undefined") || (typeof menuItem.checked !== "undefined")){
      item.checked = !!menuItem.checked;
    }

    this.events_.next({id, item});
  }

  static Install(template){
    this.template_ = template;
    this.Update();
  }

  static Update(){
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
        if( item.type !== "separator" && !item.id ) throw( "id is required for non-separator menu items");
        if( typeof item.enabled !== "undefined" ) options.enabled = item.enabled;
        if( item.items && item.items.length ) options.submenu = build_menu(item.items, options.id);
        else if(item.items) options.submenu = [];
        else options.click = this.Click.bind(this, options.id, item);
        menu.append(new MenuItem(options));
      });
      return menu;
    }

    let items = this.template_.items as LocalMenuItem[];
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

    this.loaded_.next(true);    
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
