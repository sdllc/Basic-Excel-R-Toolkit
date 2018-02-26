
import {MenuUtilities} from './menu_utilities';

/**
 * class for showing dialogs, along the lines of bootstrap's modals 
 */

const DialogTemplate = `
  <div class='dialog_container'>
    <div class='dialog_header'>
      <div class='dialog_title'></div>
      <div class='dialog_close_x'></div>
    </div>
    <div class='dialog_body'>
    </div>
    <div class='dialog_footer'>
      <button class='button' data-index='0'></button>
      <button class='button' data-index='1'></button>
      <button class='button' data-index='2'></button>
      <button class='button' data-index='3'></button>
    </div>
  </div>
`;

export interface DialogButton {
  text:string;
  data?:any;
}

export interface DialogSpec {

  title?:string;
  body?:string;

  /** buttons, as text strings (text will be returned) */
  buttons?:(string|DialogButton)[];

  /** support the escape key to close the dialog */
  escape?:boolean;
}

export class DialogManager {

  static container_node_:HTMLElement;
  static node_map_:{[index:string]: HTMLElement|HTMLElement[]} = {};

  static EnsureNodes(){

    if(this.container_node_) return;

    let node  = document.createElement("div");
    node.classList.add( "dialog_overlay");

    // ok, this is a little sloppy. create a flat map...
    node.innerHTML = DialogTemplate;
    let map_children = (element) => {
      let children = element.children;
      if(children){
        Array.prototype.forEach.call(children, x => {
          let cls = x.className;
          if( this.node_map_[cls]){
            let entry = this.node_map_[cls];
            if( Array.isArray(entry)){
              entry.push(x);
            }
            else {
              this.node_map_[cls] = [entry, x];
            }
          }
          else this.node_map_[cls] = x;
          map_children(x);
        });
      }
    }
    map_children(node);

    document.body.appendChild(node);
    this.container_node_ = node;
  
  }

  set title(title) { (DialogManager.node_map_.dialog_title as HTMLElement).textContent = title || ""; }
  set body(html) { (DialogManager.node_map_.dialog_body as HTMLElement).innerHTML = html || ""; }

  set buttons(buttons:(string|DialogButton)[]) {
    for( let i = 0; i< 4; i++ ){
      let node = DialogManager.node_map_.button[i];
      if( buttons && buttons.length > i ){
        let button = buttons[i];
        node.style.display = "inline-block";
        if( typeof button === "string" ){
          node.textContent = button;
        }
        else {
          node.textContent = button.text;
        }
      }
      else {
        node.style.display = "none";
      }
    }
  }

  // hold on to active dialog spec for callbacks
  current_dialog_spec_:DialogSpec;

  key_listener_:EventListenerObject;
  click_listener_:EventListenerObject;


  constructor(){

    DialogManager.EnsureNodes();

    this.title = "Are You Experienced";
    this.body = "Or have you ever been experienced?";
    this.buttons = ["Yes", "No", "I Have"];

  }

  Hide(){
    DialogManager.container_node_.style.display = "none";
    if( this.key_listener_ ){
      console.info( "Remove kl");
      document.removeEventListener("keydown", this.key_listener_);
      this.key_listener_ = null;
    }
    if( this.click_listener_ ){
      console.info( "Remove cl");
      DialogManager.container_node_.removeEventListener("click", this.click_listener_);
    }
    this.current_dialog_spec_ = null;
    MenuUtilities.Enable();
  }

  DelayResolution(resolve:Function, data:any){
    setTimeout(() => { 
      this.Hide(); 
      resolve(data);
    }, 1 );
  }

  Show(spec:DialogSpec){
    
   MenuUtilities.Disable();

    return new Promise((resolve, reject) => {

      this.title = spec.title||"";
      this.body = spec.body||"";
      this.buttons = spec.buttons||[];
      DialogManager.container_node_.style.display = "flex";

      this.key_listener_ = {
        handleEvent: (event) => {
          if(spec.escape){
            if((event as KeyboardEvent).key === "Escape" ){
              this.DelayResolution(resolve, {reason: "escape_key", data: null });
            }
          }
        }
      };
      document.addEventListener("keydown", this.key_listener_);

      this.click_listener_ = {
        handleEvent: (event) => {
          let target = event.target as HTMLElement;
          if(target.className === "dialog_close_x"){
            this.DelayResolution(resolve, {reason: "x", data: null});
          }
          else if(/button/i.test(target.tagName||"")){
            let index = Number(target.getAttribute("data-index")||0);
            let button = spec.buttons[index];
            let data:any = null;
            if( typeof button === "string" ) data = button;
            else data = button.data;
            this.DelayResolution(resolve, {reason: "button", data });
          }
        }
      };
      DialogManager.container_node_.addEventListener("click", this.click_listener_);
      this.current_dialog_spec_ = spec;

    });

  }

}



