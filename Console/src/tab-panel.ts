
export interface TabSpec {
  label:string;
  closeable?:boolean;
  dirty?:boolean;
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

  tabs_:TabSpec[] = [];
  active_:number = 0;

  constructor(parent:HTMLElement|string){
    
    if(typeof parent === "string") this.parent_ = document.querySelector(parent);
    else this.parent_ = parent;
    this.parent_.classList.add( "tab-panel-container");

    let children = this.parent_.children;

    // create the tab bar

    this.tab_container_ = document.createElement("div");
    this.tab_container_.classList.add( "tab-panel-tabs" );
    this.parent_.appendChild(this.tab_container_);

    // move children to the content panel (and create the content panel)

    this.tab_content_ = document.createElement("div");
    this.tab_content_.classList.add( "tab-panel-content" );
    
    Array.prototype.forEach.call(children, child => {
      this.tab_content_.appendChild(child);
    });

    this.parent_.appendChild(this.tab_content_);

    this.tab_container_.addEventListener("click", event => {
      event.stopPropagation();
      event.preventDefault();

      console.info( "click", event.target);
      this.ActivateTab((event.target as HTMLElement).parentElement);
    })

  }
  
  AddTabs(...tabs:TabSpec[]){
    tabs.forEach( element => this.tabs_.push(element));
    this.UpdateLayout();
  }

  ActivateTab(index:number|HTMLElement){
    Array.prototype.forEach.call(this.tab_container_.children, (child, i) => {
      if( i === index || index === child ) child.classList.add("tab-panel-active-tab");
      else child.classList.remove("tab-panel-active-tab");
    });
  }

  private UpdateLayout(){

    while( this.tab_container_.lastChild ) this.tab_container_.removeChild(this.tab_container_.lastChild);

    if(this.active_ < 0 ) this.active_ = 0;

    this.tabs_.forEach((tab, index) => {

      // close/dirty is a node so we can track clicks on it.
      // icon is also a node so we can lay out the various nodes
      // properly.

      let node = document.createElement("div");
      node.classList.add("tab-panel-tab");
      if( this.active_ === index ) node.classList.add("tab-panel-active-tab");

      /*
      let icon = document.createElement("div");
      icon.classList.add("tab-panel-tab-icon");
      node.appendChild(icon);
      */

      let label = document.createElement("div");
      label.classList.add("tab-panel-tab-label");
      label.textContent = tab.label;
      node.appendChild(label);

      let button = document.createElement("div");
      button.classList.add("tab-panel-tab-button");
      if( tab.dirty ) button.classList.add("tab-panel-tab-dirty");
      if( tab.closeable ) button.classList.add("tab-panel-tab-closeable");
      node.appendChild(button);

      this.tab_container_.appendChild(node);
    })

  }

}
