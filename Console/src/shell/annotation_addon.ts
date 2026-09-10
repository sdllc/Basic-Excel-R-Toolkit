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

import { Terminal } from '@xterm/xterm';
import { GetCellDimensions, GetScreenElement, OnBufferTrim } from './xterm_internals';

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

  /** one manager per terminal instance; create it once the terminal is open */
  constructor(terminal:Terminal){
    this.terminal_ = terminal;

    // positions depend on the viewport, so follow scrolling and resizing.
    // when the scrollback fills, lines drop off the top and every line
    // index shifts; that is what the top offset accounts for.

    this.terminal_.onScroll(() => this.UpdateAnnotations());
    this.terminal_.onResize(() => this.UpdateAnnotations());
    OnBufferTrim(this.terminal_, count => this.Overflow(count));

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

    const cell = GetCellDimensions(this.terminal_);

    // don't cache this one, though.

    const buffer = this.terminal_.buffer.active;

    if(!this.node_){
      this.node_ = GetScreenElement(this.terminal_);
    }

    // update positions

    // TODO: remove offscreen annotations from DOM
    // TODO: flag for removal?

    this.annotations_.forEach(annotation => {

      // TAG: switching scaled -> actual to fix highdpi
      
      let top = (annotation.line - buffer.viewportY - this.top_offset_) * cell.height;
      let left = (annotation.column||0) * cell.width;

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


}