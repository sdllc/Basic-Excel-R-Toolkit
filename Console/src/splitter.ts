
import * as Rx from 'rxjs';

/**
 * splitter orientation
 */
export enum SplitterOrientation {
  Horizontal = "horizontal", Vertical = "vertical"
}

type EventHandler = (event) => void;

export interface SplitterStatus {
  split:number;
  orientation:SplitterOrientation;
}

/**
 * html splitter window. when instantiated, it determines layout from
 * markup. markup should look like this (where #parent is the parent node):
 * 
 * <element id="parent">
 *   <element/>
 *   <element/>
 * </element>
 * 
 * we are not doing anything fancy. we expect a splitter window to have
 * two children. if there is only 1, or more than 2, behavior is unspecified.
 * an additional child will be added (the split/drag handle). class names
 * will be added to the parent and children, all prefaced with "splitter-".
 * 
 * there's also a global drag overlay element, which will be added to 
 * document.body (once).
 */
export class Splitter {

  // --- class fields ---

  /** class attached to splitter parent and overlay when dragging */
  private static get horizontal_class() { return "splitter-orientation-horizontal" }

  /** class attached to splitter parent and overlay when dragging */
  private static get vertical_class() { return "splitter-orientation-vertical" }

  /** overlay element for dragging (single) */
  private static overlay_:HTMLElement = null;

  /** sub-element for displaying layout when dragging */
  private static overlay_display_:HTMLElement = null;

  // --- class members ---

  /** the parent (splitter-window) */
  private parent_:HTMLElement;

  /** children */
  private children_:HTMLElement[] = [];

  /** splitter */
  private splitter_:HTMLElement;

  /** mouse target/handle */
  private handle_:HTMLElement;

  /** local ref to orientation */
  private orientation_:SplitterOrientation = SplitterOrientation.Horizontal;

  /** current split, as a % */
  private split_:Rx.BehaviorSubject<SplitterStatus>;

  /** value accessor */
  public get split() { return this.split_.value.split; }

  /** value accessor */
  public get orientation() { return this.split_.value.orientation; }

  /** subject accessor */
  public get status() { return this.split_; }

  /** drag events */
  private dragging_ = new Rx.BehaviorSubject<boolean>(false);

  /** accessor */
  public get dragging() { return this.dragging_; }
  
  /** cache event handlers so we can remove them */
  private event_handlers_:{[index:string]: EventHandler} = {}; 

  /**
   * 
   * @param parent 
   * @param orientation 
   * @param split as percent of total (e.g. * 100); defaults to 50 (50%)
   */
  constructor(parent:HTMLElement|string, orientation:SplitterOrientation = SplitterOrientation.Horizontal, split = 50){

    // resolve parent if string

    if( typeof parent === "string" ) this.parent_ = document.querySelector(parent);
    else this.parent_ = parent;

    // add classes to parent, children

    this.parent_.classList.add("splitter-parent");
    Array.prototype.forEach.call(this.parent_.children, child => {
      this.children_.push(child as HTMLElement);
      child.classList.add("splitter-child");
    });

    // add splitter

    this.splitter_ = document.createElement("div");
    this.splitter_.classList.add( "splitter-splitter");
    this.parent_.appendChild(this.splitter_);
    this.splitter_.addEventListener("mousedown", event => this.StartDrag(event));

    // add overlay (if this is the first splitter)

    if(null === Splitter.overlay_ ){
      Splitter.overlay_ = document.createElement("div");
      Splitter.overlay_.id = "splitter-drag-overlay";
      document.body.appendChild(Splitter.overlay_);

      // NOTE: can't use a pseudo-element because we can't get
      // a handle to it to update the value.

      Splitter.overlay_display_ = document.createElement("div");
      Splitter.overlay_.appendChild(Splitter.overlay_display_);
    }

    // create subject for split. that subject also is the de-facto
    // accessor for the current split value

    this.split_ = new Rx.BehaviorSubject<SplitterStatus>({
      split, orientation: this.orientation_ });

    // set orientation for any switches

    this.orientation_ = orientation;

    // update classes for orientation; this also sets the initial split widths

    this.UpdateClasses();

  }

  /**
   * end drag: clean up event handlers, broadcast
   */
  private EndDrag(){
    Splitter.overlay_.removeEventListener("mousemove", this.event_handlers_.move);
    Splitter.overlay_.removeEventListener("mouseleave", this.event_handlers_.leave);
    Splitter.overlay_.removeEventListener("mouseup", this.event_handlers_.up);
    Splitter.overlay_.classList.remove("visible");
    this.event_handlers_ = {};
    this.dragging_.next(false);
  }

  /** 
   * start drag: show overlay, set up event handlers 
   */
  private StartDrag(start_event:any){

    this.splitter_.classList.add("splitter-dragging");

    if( this.orientation_ === SplitterOrientation.Horizontal ){
      Splitter.overlay_.classList.add(Splitter.horizontal_class);
      Splitter.overlay_.classList.remove(Splitter.vertical_class);
    }
    else {
      Splitter.overlay_.classList.add(Splitter.vertical_class);
      Splitter.overlay_.classList.remove(Splitter.horizontal_class);
    }

    Splitter.overlay_.classList.add("visible");
    
    let round = x => Math.round(x*1000)/10;

    let width = Splitter.overlay_.offsetWidth;
    let height = Splitter.overlay_.offsetHeight;

    if( this.orientation_ === SplitterOrientation.Horizontal ){
      let current_split = round(start_event.clientX / width);
      this.event_handlers_.move = event => {
        event.stopPropagation();
        event.preventDefault();
        let drag_split = round(event.clientX / width);
        if( drag_split !== current_split ){
          current_split = drag_split;
          this.Update("width", drag_split);
        }
      }
    }
    else {
      let current_split = round(start_event.clientY / height);
      this.event_handlers_.move = event => {
        event.stopPropagation();
        event.preventDefault();
        let drag_split = round(event.clientY / height);
        if( drag_split !== current_split ){
          current_split = drag_split;
          this.Update("height", drag_split);
        }
      }
    }

    this.event_handlers_.leave = event => this.EndDrag();
    this.event_handlers_.up = event => this.EndDrag();

    Splitter.overlay_.addEventListener("mousemove", this.event_handlers_.move);
    Splitter.overlay_.addEventListener("mouseleave", this.event_handlers_.leave);
    Splitter.overlay_.addEventListener("mouseup", this.event_handlers_.up);

    this.dragging_.next(true);
    
  }

  /**
   * set split and update layout
   */
  Set(split:number){
    this.Update( 
      this.orientation_ === SplitterOrientation.Vertical ? "height" : "width", 
      this.split_.value.split);
  }

  /**
   * set orientation and update layout
   */
  SetOrientation(orientation:SplitterOrientation){
    this.orientation_ = orientation;
    this.UpdateClasses();
  }

  /**
   * update layout, including classes 
   */
  private UpdateClasses(){
    if(this.orientation_ === SplitterOrientation.Vertical ){
      this.parent_.classList.remove(Splitter.horizontal_class);
      this.parent_.classList.add(Splitter.vertical_class);
      this.children_[0].style.width = "";
      this.children_[1].style.width = "";
      this.Update("height", this.split_.value.split);
    }
    else {
      this.parent_.classList.remove(Splitter.vertical_class);
      this.parent_.classList.add(Splitter.horizontal_class);
      this.children_[0].style.height = "";
      this.children_[1].style.height = "";
      this.Update("width", this.split_.value.split);
    }
  }

  /**
   * update layout only (no class changes)
   */
  private Update(key:string, split:number){
    this.children_[0].style[key] = `${split}%`;
    this.children_[1].style[key] = `${100-split}%`;
    Splitter.overlay_display_.textContent = `${split.toFixed(1)}%`;  
    this.split_.next({split, orientation: this.orientation_});
  }

}
