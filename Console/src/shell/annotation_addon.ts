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

// we can use this for importing type info, but it will break the 
// production build. xterm's ts integration is somewhat iffy atm.
// import {Terminal} from "xterm/src/Terminal";

// for build
type Terminal = any;

/**
 * addon for annotations (html nodes) in xtermjs. the basic idea 
 * is that we can attach an arbitrary html element to a text position. 
 * 
 * there's an underlying assumption that the container for the xterm
 * node has relative positioning, otherwise the elements won't be 
 * positioned properly. it should also have overflow hidden, or they'll
 * overflow.
 * 
 * we're adding absolute positioning to nodes via style, rather than
 * attaching a class name. we also use style for top and (optionally) 
 * left.
 * 
 * TODO: capture when selected [?]
 * TODO: copy as html, including annotations?
 * 
 * ---
 * 
 * FIXME: I have an idea for a more efficient implementation. create 
 * a giant overlay on top of the terminal and clip with overflow. then
 * attach nodes to that. it means we only have to move the parent and 
 * everything else will move in turn. will screw up mouse handling, 
 * though (you can declare mouseevents:none, but then we'll lose mouse 
 * events on annotations).
 * 
 * something to think about.
 * 
 */

export interface AnnotationInfo {
  height:number;
  element:HTMLElement;
}

export interface AnnotationType {

  /** insert at line (required) */
  line:number;

  /** insert at column (optional, defaults to zero) */
  column?:number;

  /** the node */
  element:HTMLElement;

  /** 
   * this can be used to delete if you don't want to hold on to the 
   * annotation struct or even the node
   */
  id?:any;
}

interface AnnotationTypeInternal extends AnnotationType {
  
  /** internal flag */
  attached?: boolean;
}

/**
 * class implementation, also the factory for attaching to xterm instances
 */
export class AnnotationManager {

  /** cache */
  private annotations_:AnnotationTypeInternal[] = [];

  /** accessor */
  public get annotations() : AnnotationType[] { return this.annotations_ }

  /** ref */
  private terminal_:Terminal;

  /** top of buffer, in case it overflows */ 
  private top_offset_ = 0;

  /** attach to node */
  private node_:HTMLElement;

  /** constructor is private; use factory (via accessor) */
  private constructor(terminal:Terminal){
    this.terminal_ = terminal;

    // hook up events. we're interested in scrolling, but that's not 
    // implemented (at least not as one would expect). @see #657.

    (this.terminal_.viewport as any).viewportElement.addEventListener('scroll', e => {
      this.UpdateAnnotations();
    });

  }

  /** 
   * dummy. intended to be called when an xterm instance is created or 
   * instantiated. we could just override Open, but this way it's optional
   * on a per-instance basis (which is useful since apply acts globally).
   */
  public Init(){}

  /** updates element positions, inserting into DOM if necessary */
  private UpdateAnnotations(){

    // FIXME: we could cache this, if we had a way of getting 
    // notified on style chages. is that a thing? OTOH, this is 
    // not expensive. optimize somewhere else.

    let dimensions = this.terminal_.renderer.dimensions;

    // don't cache this one, though.

    let buffer = this.terminal_.buffer;

    if(!this.node_){
      this.node_ = ((this.terminal_ as any).parent as HTMLElement).querySelector(".xterm-screen");
    }

    // update positions

    // TODO: remove offscreen annotations from DOM
    // TODO: flag for removal?

    this.annotations_.forEach(annotation => {

      // TAG: switching scaled -> actual to fix highdpi
      
      let top = (annotation.line - buffer.ydisp - this.top_offset_) * dimensions.actualCellHeight;
      let left = (annotation.column||0) * dimensions.actualCellWidth; 

      if(!annotation.attached){
        annotation.element.style.position = "absolute";
        this.node_.appendChild(annotation.element);
        annotation.attached = true;
      }

      annotation.element.style.top = `${top}px`;
      if(annotation.column){
        annotation.element.style.left = `${left}px`;
      }
    });

  }

  /** explicitly sets (or resets) top offset */
  public SetTopOffset(offset = 0){
    this.top_offset_ = offset;
    this.UpdateAnnotations();
  }

  /** updates offset when we overflow */
  public Overflow(count = 1){
    this.top_offset_ += count;
    
    // this is called when the buffer is modified, but before 
    // anything else happens. we can rely on subsequent events
    // to trigger layout updates.

    // BUT this might be a good time to check if something
    // has gone offscreen, or at least flag someone else to do so

  }

  public AddAnnotation(annotation:AnnotationType){
    this.annotations_.push(annotation);
    this.UpdateAnnotations();
  }

  /**
   * remove annotation by instance, node, or ID
   */
  public RemoveAnnotation(annotation:any){
    this.annotations_ = this.annotations_.filter(x => {
      if(x === annotation || x.id === annotation || x.element === annotation){
        if( x.element.parentElement ){
          x.element.parentElement.removeChild(x.element);
        }
        return false;
      }
      return true;
    });
  }

  /**
   * remove all annotations. 
   */
  public RemoveAnnotations(){
    this.annotations_.forEach(x => {
      if(x.element.parentElement){
        x.element.parentElement.removeChild(x.element);
      }
    });
    this.annotations_ = [];
  }

  static apply(terminalConstructor) {

    // NOTE: the business with the top offset is not going to work 
    // if the instance is not created before scrolling over the end. 
    // so we will need to create beforehand, which will require 
    // overloading some method. or you can just be very disciplined 
    // and call the accessor when you create the thing. 
    //
    // [UPDATE: that's what the Init method is for]

    Object.defineProperty(terminalConstructor.prototype, "annotation_manager", {

      // NOTE: don't use an arrow function here as it will rewrite `this`

      get: function(){ 
        if(!this.annotation_manager_){
          this.annotation_manager_ = new AnnotationManager(this);
        }
        return this.annotation_manager_;
      }

    });

    // override scroll so we can get clean overflow events.

    let scroll_function = terminalConstructor.prototype.scroll;
    terminalConstructor.prototype.scroll = function(arg){

      // this test is duplicated from the scroll function, which
      // checks for overflow

      if ( this.annotation_manager
        && this.buffer.scrollTop === 0 
        && (this.buffer.lines.length === this.buffer.lines.maxLength)){
        this.annotation_manager.Overflow();
      }

      scroll_function.call(this, arg);      
    };

    // extend "clear" to remove annotations, reset overflow

    let clear_function = terminalConstructor.prototype.clear;
    terminalConstructor.prototype.clear = function(){
      clear_function.call(this);
      if(this.annotation_manager){
        this.annotation_manager.RemoveAnnotations();
        this.annotation_manager.SetTopOffset(); // reset w/ default value
      }
    };

  }

}