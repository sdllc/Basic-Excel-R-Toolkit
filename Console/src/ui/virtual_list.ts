
export interface FormatItemFunction {(node, array, index, width?)};
export interface ClickFunction {(index)};

interface RenderFunction {(start)};

interface ScrollFunction {(index)};

export interface ListOptions {

  // callback for formatting a single item in the list. this list
  // supports variable-sized items.
  format:FormatItemFunction;

  // data passed to the format function. not necessarily an array
  // (at least for mirrors, a column-oriented list of arrays)
  data:any;

  // length of data.
  length:number;

  // the scroll node -- this one has scrollbars and generates events
  outer_node:HTMLElement;

  // containing class name, so we can set the measurement node to match
  containing_class_name?:string;

  // click callback. returns a node. you can attach some data to the 
  // node in format, and then check it in click.
  click?:ClickFunction;

}

export class VirtualList {

  /** 
   * generated function for rendering, exported so we can call it 
   * from a public Update function
   */
  private render_:RenderFunction;

  /**
   * generated function for scroll into view
   */
  private scroll_:ScrollFunction;

  /**
   * generated function to remove listeners
   */
  private cleanup_:Function;

  /** the first rendered item in the list */
  private first_ = 0;

  static EnsureMeasurementNode(){
    let measurement_node = document.querySelector("#list-measurement-node");
    if(!measurement_node) {
      measurement_node = document.createElement("div");
      measurement_node.id = "list-measurement-node";
      document.body.appendChild(measurement_node);
    }
    return measurement_node;
  }

  constructor(){}

  /** calls the paint method */
  Repaint(start = this.first_){
    this.render_(start);
  }

  /** scroll into view */
  ScrollIntoView(index = 0){
    this.scroll_(index);
  }

  /** 
   * removes event handlers
   */
  CleanUp(){
    if(this.cleanup_){
      this.cleanup_();
      this.cleanup_ = null;
    }
    this.scroll_ = null;
    this.render_ = null;
  }

  /**
   * creates the list, and returns the node. that design is intended to 
   * facilitate cleanup, particularly if we're changing the list on the fly
   */
  CreateList(options:ListOptions) : HTMLElement{ 

    let inner_node = document.createElement("div");
    inner_node.className = "list-inner-body";

    let measurement_node = VirtualList.EnsureMeasurementNode();
    measurement_node.innerHTML = "";
    measurement_node.className = options.containing_class_name || ""; 
    let temp = document.createElement("div");
    measurement_node.appendChild(temp);

    let data = options.data;
    let length = options.length;

    // measure

    let height = new Array(length);

    let max_width = 100;
    let min_height = 1000;
    let total_height = 0;

    for( let i = 0; i< length; i++ ){
      options.format(temp, data, i);
      let rect = temp.getBoundingClientRect();
      max_width = Math.max(max_width, rect.width);
      min_height = Math.min(min_height, rect.height);
      height[i] = rect.height;
      total_height += rect.height;
    }

    inner_node.style.height = total_height + "px";
    inner_node.style.width = max_width + "px";

    let count = Math.ceil(options.outer_node.offsetHeight / min_height) * 2;
    let buffer = Math.round(count/4);
    let nodes = [];

    for( let i = 0; i< count; i++ ){
      let entry = document.createElement("div");
      options.format(entry, data, i, max_width);
      inner_node.appendChild(entry);
      nodes.push(entry);
    }

    // paint function
    this.render_ = (start) => {
      start = start - (start%2); // always even, for css
      let top = 0;
      for( let i = 0; i< start && i< length; i++ ) top += height[i];
      for( let i = 0; i< count && (i+start < length); i++ ){
        options.format(nodes[i], data, i+start)
      }
      nodes[0].style.marginTop = top + "px";
    }

    // scroll function (scroll item into view)
    this.scroll_ = (selected) => {
      let top = 0;
      for( let i = 0; i< selected; i++ ){
        top += height[i];
      }      
      options.outer_node.scrollTop = top;
    }

    // scroll event handler
    let updating = false;
    let scroll_handler = (e) => {
      if(!updating){
        updating = true;
        requestAnimationFrame(() => {
          let top = options.outer_node.scrollTop;
          let bottom = 0;
          for(let start = 0; start< height.length; start++ ){
            bottom = bottom + height[start];
            if( bottom > top ){
              if((start - (start % 2)) !== this.first_){
                this.first_ = Math.max(0, start - buffer);
                requestAnimationFrame(() => {this.render_(this.first_)});
              }
              break;
            }
          }
          updating = false;
        });
      }
    };

    // attach event handler 
    options.outer_node.addEventListener("scroll", scroll_handler);

    let click_handler = (e) => {
      console.info("Click (only should be 1)");
      let target = e.target as HTMLElement;
      while(target && !target.classList.contains("list-body-entry")){
        target = target.parentElement;
      }
      if(target) {
        options.click(target);
      }
    };

    if(options.click){
      options.outer_node.addEventListener("click", click_handler);
    }

    this.cleanup_ = function(){
      options.outer_node.removeEventListener("scroll", scroll_handler);
      options.outer_node.removeEventListener("click", click_handler);
    }

    return inner_node;

  }

}

