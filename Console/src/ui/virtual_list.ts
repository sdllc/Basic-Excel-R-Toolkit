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

export interface FormatItemFunction {(node, array, index, width?)};
export interface ClickFunction {(index, event)};

interface RenderFunction {(start)};
interface ScrollFunction {(index)};

export interface ListOptions {

  // callback for formatting a single item in the list. this list
  // supports variable-sized items.
  format:FormatItemFunction;

  // data passed to the format function. not necessarily an array
  // (at least for mirrors, a column-oriented list of arrays).
  //
  // FIXME: this is unecessary, because we are passing in the format
  // function. just do it on the other side.
  //
  data:any;

  // length of data. required because data is not necessarily an array
  data_length:number;

  // the scroll node -- this one has scrollbars and generates events
  outer_node:HTMLElement;

  // containing class name, so we can set the measurement node to match
  containing_class_name?:string;

  // click callback. returns a node. you can attach some data to the 
  // node in format, and then check it in click.
  click?:ClickFunction;

  // option: fixed height. this allows us to speed up measurement, 
  // particularly if we update. 
  fixed_height?:boolean;

  // which one to measure for fixed-height lists (generally going to be 
  // because of width, if we want to push out to the widest node)
  hint?:number;

}

/** 
 * virtual list. some hacks for really large lists (like the R packages
 * list) so we can filter in reasonably close to real time.
 */
export class VirtualList {

  /**
   * generated function to remove listeners
   */
  private cleanup_:Function;

  private options_:ListOptions;

  private nodes_:HTMLElement[] = [];

  private tops_:number[] = [];
  private height_:number[] = [];

  private max_width_ = 0;
  private min_height_ = 0;
  private total_height_ = 0;

  private inner_node_:HTMLElement;

  /** the first rendered item in the list */
  private first_ = 0;

  private static measurement_node_:HTMLElement;

  static EnsureMeasurementNode(){
    if(!this.measurement_node_ ){
      this.measurement_node_ = document.createElement("div");
      this.measurement_node_.id = "list-measurement-node";
      document.body.appendChild(this.measurement_node_);
    }
  }

  constructor(){}

  /** calls the paint method */
  Repaint(start = this.first_){
    this.Render(start);
  }

  /** scroll into view */
  ScrollIntoView(index = 0){
    if(this.options_ && this.tops_ && this.tops_.length){
      this.options_.outer_node.scrollTop = this.tops_[index];
    }
  }

  private Render(start = 0){
    
    // keep odd/even ordering for css
    start = start - (start%2); 

    // render nodes
    for( let i = 0; i< this.nodes_.length && (i+start < this.options_.data_length); i++ ){
      this.options_.format(this.nodes_[i], this.options_.data, i+start)
    }

    // push down
    this.nodes_[0].style.marginTop = this.tops_[start] + "px";

  }

  /** 
   * removes event handlers, nodes
   */
  CleanUp(){
    if(this.cleanup_){
      this.cleanup_();
      this.cleanup_ = null;
    }
    // this.scroll_ = null;
    // this.render_ = null;
    this.options_ = null;
    this.nodes_ = [];
  }

  /**
   * measures nodes and calculates layout. this might get done 
   * more than once if we update data.
   */
  private Measure(options:ListOptions, updating = false){

    VirtualList.EnsureMeasurementNode();
    VirtualList.measurement_node_.className = options.containing_class_name || ""; 

    let test_node;

    if(!updating){
      VirtualList.measurement_node_.innerText = "";
      test_node = document.createElement("div");
      VirtualList.measurement_node_.appendChild(test_node);
    }
    else test_node = VirtualList.measurement_node_.firstChild;

    let tops = new Array(options.data_length);
    let height = new Array(options.data_length);

    let max_width = 100;
    let min_height = 1000;
    let total_height = 0;

    let data = options.data;
    let data_length = options.data_length;

    if( this.options_.fixed_height){

      // since we are using shared nodes, this is not necessarily 
      // going to be a safe assumption. 
      // FIXME: use an instance-specific child node. 

      if(!updating) options.format(test_node, data, options.hint||0);

      let rect = test_node.getBoundingClientRect();
      max_width = Math.max(max_width, rect.width);
      min_height = Math.min(min_height, rect.height);

      for( let i = 0; i< data_length; i++ ){
        height[i] = rect.height;
        tops[i] = total_height;
        total_height += rect.height;
      }

    }
    else {
      for( let i = 0; i< data_length; i++ ){
        options.format(test_node, data, i);
        let rect = test_node.getBoundingClientRect();
        max_width = Math.max(max_width, rect.width);
        min_height = Math.min(min_height, rect.height);
        height[i] = rect.height;
        tops[i] = total_height;
        total_height += rect.height;
      }
    }

    this.tops_ = tops;
    this.height_ = height;
    this.max_width_ = max_width;
    this.min_height_ = min_height;
    this.total_height_ = total_height;

    this.inner_node_.style.height = this.total_height_ + "px";
    this.inner_node_.style.width = this.max_width_ + "px";

  }

  /**
   * update. generally for filtering.
   */
  Update(options:ListOptions){

    // FIXME: this may be generating two paint calls 

    this.options_ = options;
    this.Measure(options, true);
    this.ScrollIntoView(0);
    requestAnimationFrame(() => {
      this.first_ = 0;
      this.Repaint();
    })
  }

  /**
   * creates the list, and returns the node. that design is intended to 
   * facilitate cleanup, particularly if we're changing the list on the fly
   */
  CreateList(options:ListOptions) : HTMLElement{ 

    this.options_ = options;

    this.inner_node_ = document.createElement("div");
    this.inner_node_.className = "list-inner-body";

    this.Measure(options);

    let data = options.data;
    let data_length = options.data_length;
    
    let count = Math.ceil(options.outer_node.offsetHeight / this.min_height_) * 2;

    // create initial nodes; this does first-pass formatting

    let nodes = [];
    for( let i = 0; i< count; i++ ){
      let entry = document.createElement("div");
      options.format(entry, data, i, this.max_width_);
      this.inner_node_.appendChild(entry);
      nodes.push(entry);
    }
    this.nodes_ = nodes;

    // scroll function (scroll item into view). this gets attached 
    // to the outside node, which does the scrolling

    // scroll event handler
    let updating = false;
    let scroll_handler = (e) => {

      if(!updating){
        updating = true;
        requestAnimationFrame(() => {

          let top = options.outer_node.scrollTop;
          let first = this.first_;
          let tops = this.tops_;

          // this is better but it's going to jump if you 
          // change directions, could be smarter about it

          if( tops[first] < top ){
            for( ++first; first < tops.length && tops[first] < top; first++ );
          }
          else {
            for( --first; first >= 0 && tops[first] > top; first-- );
          }

          // ensure there's a buffer
          first = Math.max(0, first - 2); // fixme: count/4
         
          // only paint if we're shifting. 
          // FIXME: maybe set first_ anyway for the next scroll event?
          
          if((first - (first % 2)) !== this.first_){
            this.first_ = first;
            this.Render(this.first_);
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
        e.stopPropagation();
        e.preventDefault();
        options.click(target, e);
      }
    };

    let mousedown_handler = (e) => {
      e.stopPropagation();
      e.preventDefault();
    }

    options.outer_node.addEventListener("mousedown", mousedown_handler);

    if(options.click){
      options.outer_node.addEventListener("click", click_handler);
    }

    this.cleanup_ = function(){
      options.outer_node.removeEventListener("scroll", scroll_handler);
      options.outer_node.removeEventListener("mousedown", mousedown_handler);

      // this should be OK even if it was not attached
      options.outer_node.removeEventListener("click", click_handler);

      this.inner_node_ = null;
    }

    return this.inner_node_;

  }

}

