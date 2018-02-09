
import {Terminal} from "xterm/src/Terminal";

/**
 * addon for annotations (html nodes) in xtermjs. the basic idea 
 * is that we can attach an arbitrary node to a text position. 
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
 */

export interface AnnotationType {
  line:number;
  column?:number;
  element:HTMLElement;
  attached?:boolean;
}

export class AnnotationManager {

  /** cache */
  private annotations_:AnnotationType[] = [];

  /** accessor */
  public get annotations() : AnnotationType[] { return this.annotations_ }

  /** ref */
  private terminal_:Terminal;

  /** top of buffer, in case it's adjusted */
  private top_offset_ = 0;

  /** use factory (via accessor) */
  private constructor(terminal:Terminal){
    this.terminal_ = terminal;

    // hook up events. we're interested in scrolling, but that's not 
    // implemented (at least not as one would expect). @see #657:

    (this.terminal_.viewport as any).viewportElement.addEventListener('scroll', e => {
      this.UpdateAnnotations();
    });
    
    // the actual scroll event can be used to check for overflow. it
    // seems to get triggered on every line scroll, not batched. that
    // actually works out ok for us (assume this will probably change,
    // though).

    // FIXME: it's possible an annotation has been shifted completely
    // out of visible scope. in that case we can dispose of it, or at 
    // least stop rendering it. maybe that should be handled in the 
    // update method, just to centralize.

    this.terminal_.on("scroll", (ydisp) => {
      let scrollback = this.terminal_.options.scrollback;
      if( scrollback && ydisp >= scrollback ) this.top_offset_++;
    });

  }

  private UpdateAnnotations(){

    // FIXME: we could cache this, if we had a way of getting notified 
    // on style chages. is that a thing?

    let dimensions = this.terminal_.renderer.dimensions;

    // don't cache this one, though.

    let buffer = this.terminal_.buffer;

    // update positions

    // TODO: remove offscreen annotations from DOM
    // TODO: flag for removal?

    this.annotations_.forEach(annotation => {

      // FIXME: there's a little error, I think added when the offset changes.

      let top = (annotation.line - buffer.ydisp - this.top_offset_) * dimensions.scaledCellHeight;
      let left = (annotation.column||0) * dimensions.scaledCellWidth; 

      if(!annotation.attached){
        annotation.element.style.position = "absolute";
        ((this.terminal_ as any).parent as HTMLElement).appendChild(annotation.element);
        annotation.attached = true;
      }

      annotation.element.style.top = `${top}px`;
      if(annotation.column){
        annotation.element.style.left = `${left}px`;
      }
    });

  }

  public SetTopOffset(){

  }

  public AddAnnotation(annotation:AnnotationType){
    this.annotations_.push(annotation);
    this.UpdateAnnotations();
  }

  public RemoveAnnotation(annotation:AnnotationType){
    this.annotations_ = this.annotations_.filter(x => {
      if(x === annotation){
        if( x.element.parentElement ){
          x.element.parentElement.removeChild(x.element);
        }
        return false;
      }
      return true;
    });
  }

  public RemoveAnnotations(){
    this.annotations_.forEach(x => {
      if(x.element.parentElement){
        x.element.parentElement.removeChild(x.element);
      }
    });
    this.annotations_ = [];
  }

  static apply(terminalConstructor) {

    Object.defineProperty(terminalConstructor.prototype, "annotation_manager", {

      // NOTE: don't use an arrow function here as it will rewrite `this`

      get: function(){ 
        if(!this.annotation_manager_){
          this.annotation_manager_ = new AnnotationManager(this);
        }
        return this.annotation_manager_;
      }

    });

  }

}