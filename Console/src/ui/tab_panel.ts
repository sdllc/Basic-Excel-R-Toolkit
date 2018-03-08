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

import * as Rx from 'rxjs';

export interface TabSpec {

  /** tab label */
  label:string;

  /** tooltip. html title, ideally something better */
  tooltip?:string;

  /** should only be set by tab panel */
  active?:boolean;

  /** can be closed with a click (shows X) */
  closeable?:boolean;

  /** dirty: shows icon */
  dirty?:boolean;

  /** show a button. needed for close/dirty */
  button?:boolean;

  /** show an icon */
  icon?:boolean;

  /** icon class (opaque, attached to the icon node) */
  icon_class?:string;

  /** arbitrary tab data */
  data?:any;

}

export enum TabClasses {
  active = "active", 
  dirty = "dirty", 
  closeable = "closeable" 
}

export enum TabJustify {
  left = "left",
  right = "right"
}

export enum TabEventType {
  activate = "activate",
  deactivate = "deactivate",
  rightClick = "right-click",
  buttonClick = "button-click"
}

export interface TabEvent {
  type:TabEventType;
  tab:TabSpec; 
}

interface DecoratedElement extends HTMLElement {
  ref_?:TabSpec;
}

/**
 * preliminary implementation of generic tab panel. the aim is to abstract
 * tabs from implementation, and then use subscribers/observers to handle 
 * events.
 */
export class TabPanel {

  parent_:HTMLElement;
  tab_container_:HTMLElement;
  tab_content_:HTMLElement;

  /** list of tabs */
  private tabs_:TabSpec[] = [];

  public get tabs(){ return this.tabs_; }

  /** return a list of data items */
  public get data(){ return this.tabs_.map( x => x.data ); }
  
  private active_index_ = -1;

  /** observable for tab events */
  private events_:Rx.Subject<TabEvent> = new Rx.Subject<TabEvent>();

  /** accessor */
  public get events() { return this.events_; }

  public get count(){ return this.tabs_.length; }

  /** accessor: returns tab or null */
  get active() { 
    if( this.active_index_ < 0 || this.active_index_ >= this.tabs_.length ) return null;
    return this.tabs_[this.active_index_];
  }

  constructor(parent:HTMLElement|string, justify:TabJustify = TabJustify.left){
    
    if(typeof parent === "string") this.parent_ = document.querySelector(parent);
    else this.parent_ = parent;
    this.parent_.classList.add( "tab-panel-container");

    // was bug: the actual `children` list is live, so if we use 
    // it below, it will include stuff added later. here we copy 
    // the list so we just get the snapshot at T=0.
    
    let children = Array.prototype.map.call(this.parent_.children, x => x);

    // create the tab bar

    this.tab_container_ = document.createElement("div");
    this.tab_container_.classList.add( "tab-panel-tabs" );
    if( justify === TabJustify.right ) this.tab_container_.classList.add('tab-panel-justify-right');

    this.parent_.appendChild(this.tab_container_);

    // move children to the content panel (and create the content panel)

    this.tab_content_ = document.createElement("div");
    this.tab_content_.classList.add( "tab-panel-content" );
    
    children.forEach(child => this.tab_content_.appendChild(child));

    this.parent_.appendChild(this.tab_content_);

    this.tab_container_.addEventListener("mousedown", event => {

      // only left/right
      if( event.buttons !== 1 && event.button !== 2 ) return;

      event.stopPropagation();
      event.preventDefault();

      let target = event.target as HTMLElement;
 
      let button_click = (target && target.classList && target.classList.contains("tab-panel-tab-button"));

      while( target && target.classList && !target.classList.contains("tab-panel-tab")){
        if(target.classList.contains("tab-panel-tabs")) return;
        target = target.parentElement;
      }

      if(target) {
        let tab = (target as DecoratedElement).ref_;
        if( event.buttons === 1 ){
          if(button_click) {
            this.events_.next({ type: TabEventType.buttonClick, tab })
          }
          else {
            this.ActivateTab(target);
          }
        }
        else {
          this.events_.next({ type: TabEventType.rightClick, tab })
        }
      }

    })
    
  }
  
  /**
   * for context menu or other event handling
   */
  public TabFromNode(target:HTMLElement) : TabSpec {
    while(target){
      if(target === this.tab_container_) break;
      let decorated = target as DecoratedElement;
      if(decorated.ref_) return decorated.ref_;
      target = target.parentElement;
    }
    return null;
  }
  
  AppendChildNode(node:HTMLElement){
    this.tab_content_.appendChild(node);
  }

  /** selects the next tab (w/ rollover) */
  Next(){
    if(!this.tabs_||!this.tabs_.length) return;
    this.ActivateTab((this.active_index_ + 1) % this.tabs_.length);
  }

  /** selects the previous tab (w/ rollover) */
  Previous(){
    if(!this.tabs_||!this.tabs_.length) return;
    this.ActivateTab((this.active_index_ + this.tabs_.length - 1) % this.tabs_.length);
  }

  /**
   * use the flag if you are running multiple operations
   */
  ClearTabs(do_layout = true){
    this.tabs_ = [];
    if(do_layout) this.UpdateLayout();
  }

  InsertTab(tab:TabSpec, position:number, update=true){
    if( position < 0 ) position = 0;
    this.tabs_.splice(position, 0, tab);
    if(update) this.UpdateLayout();
  }

  AddTab(tab:TabSpec, update=true){
    this.tabs_.push(tab);
    if(update) this.UpdateLayout();
  }

  AddTabs(...tabs:(TabSpec|TabSpec[])[]){
    tabs.forEach( element => {
      if( Array.isArray(element)) element.forEach( x => this.tabs_.push(x));
      else this.tabs_.push(element)
    });
    this.UpdateLayout();
  }

  /**
   * remove tab, by reference
   */
  RemoveTab(tab:TabSpec){
    this.tabs_ = this.tabs_.filter(compare => compare !== tab);

    let children = this.tab_container_.children;
    for( let i = 0; i< children.length; i++ ){
      let child = children[i] as DecoratedElement;
      if( child.ref_ === tab ){
        this.tab_container_.removeChild(child);
        break;
      }
    }
   
  }

  UpdateTab(tab:TabSpec){

    // FIXME: unify all class-setting code 

    Array.prototype.some.call(this.tab_container_.children, (child, i) => {
      let check_tab = (child as DecoratedElement).ref_;
      if( tab === check_tab ){
        if( tab.dirty ) child.classList.add(TabClasses.dirty);
        else child.classList.remove(TabClasses.dirty);
        let label = child.querySelector('.tab-panel-tab-label');
        if(label){
          label.setAttribute("title", tab.tooltip || "");
          label.textContent = tab.label || "";
        }
        return true;
      }
      return false;
    });

  }

  ActivateTab(index:number|HTMLElement|TabSpec|string){

    if( typeof index === "string"){
      let s:string = index; // type guards have limited effect
      this.tabs_.some((spec, i) => {
        if( spec.label === s){
          index = i;
          return true;
        }
        return false;
      });
    }

    let active = -1;
    Array.prototype.forEach.call(this.tab_container_.children, (child, i) => {
      let tab = (child as DecoratedElement).ref_;
      if( i === index || index === child || (this.tabs_.length > i && index === this.tabs_[i])){
        child.classList.add(TabClasses.active);
        active = i;
        tab.active = true;
      }
      else {
        child.classList.remove(TabClasses.active);
        tab.active = false;
      }
    });

    // are there any tabs?
    if(!this.tabs_.length) return;

    // if there are tabs, guarantee that one is active.
    if( active < 0 && this.tabs_.length > 0 ) return this.ActivateTab(0);

    // send a deactivate (not cancelable)
    if( this.active_index_ >= 0 && this.active_index_ < this.tabs_.length ){
      this.events_.next({ type: TabEventType.deactivate, tab: this.tabs_[this.active_index_]});
    }

    // update index and send an activate
    this.active_index_ = active;
    this.events_.next({ type: TabEventType.activate, tab: this.tabs_[active]});

  }

  UpdateLayout(){

    while( this.tab_container_.lastChild ) this.tab_container_.removeChild(this.tab_container_.lastChild);

    this.tabs_.forEach((tab, index) => {

      // tab properties are defined on the tab element
      
      let node = document.createElement("div") as DecoratedElement;
      node.ref_ = tab;
      node.classList.add("tab-panel-tab");

      if( tab.dirty ) node.classList.add(TabClasses.dirty);
      if( tab.closeable ) node.classList.add(TabClasses.closeable);

      // icon classes, if any, are defined on the icon itself

      if(tab.icon){
        let icon = document.createElement("div");
        icon.classList.add("tab-panel-tab-icon");
        if( tab.icon_class ) icon.classList.add(tab.icon_class);
        node.appendChild(icon);
      }
        
      let label = document.createElement("div");
      label.classList.add("tab-panel-tab-label");
      label.textContent = tab.label;
      if(tab.tooltip) label.setAttribute("title", tab.tooltip);
      node.appendChild(label);

      if(tab.button){
        let button = document.createElement("div");
        button.classList.add("tab-panel-tab-button");
        node.appendChild(button);
      }

      this.tab_container_.appendChild(node);

    });

    //if(this.tabs_.length > 0) 
    this.ActivateTab(this.active_index_||0);

  }

}
