
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
    </div>
  </div>
`;

export interface DialogButton {
  text:string;
  data?:any;
  default?:boolean;
}

export interface DialogSpec {

  title?:string;
  body?:string;

  /** buttons, as text strings (text will be returned) */
  buttons?:(string|DialogButton)[];

  /** support the escape key to close the dialog */
  escape?:boolean;

  /** support the enter key to close the dialog */
  enter?:boolean;

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
    let container = DialogManager.node_map_.dialog_footer as HTMLElement;
    if(!container) return;

    container.textContent = "";

    buttons.forEach((spec, index) => {
      let button = document.createElement("button") as HTMLElement;
      button.classList.add("button");
      if( typeof spec === "string") button.innerText = spec;
      else {
        button.innerText = spec.text;
        if(spec.default) button.classList.add("default");
      }
      button.setAttribute("data-index", String(index))
      container.appendChild(button);
    });

  }

  // hold on to active dialog spec for callbacks
  current_dialog_spec_:DialogSpec;

  key_listener_:EventListenerObject;

  click_listener_:EventListenerObject;


  constructor(){
    DialogManager.EnsureNodes();
  }

  Hide(){
    DialogManager.container_node_.style.display = "none";
    if( this.key_listener_ ){
      document.removeEventListener("keydown", this.key_listener_);
      this.key_listener_ = null;
    }
    if( this.click_listener_ ){
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

  Test(){
    this.Show({
      title: "Dialog Title",
      escape: true, enter: true,
      body: `Accent dialog.  Text body. <br/><br/>
              Support for HTML, with some basic layout. Perhaps a few<br/> 
              types for specific widgets (we will definitely need a list).
              <br/><br/>
              The dialog should pad out on very long or very tall content<br/>
              (which is good), but that requires doing manual text layout <br/>
              (which is bad). Maybe have some styles with fixed widths.
            `,
      buttons: ["Cancel", {text: "OK", default:true}],
    }).then(x => {
      console.info(x);
    });
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
          console.info(event);
          if(spec.escape){
            if((event as KeyboardEvent).key === "Escape" ){
              this.DelayResolution(resolve, {reason: "escape_key", data: null });
            }
          }
          if(spec.enter){
            if((event as KeyboardEvent).key === "Enter" ){
              this.DelayResolution(resolve, {reason: "enter_key", data: null });
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
            else data = button.data||button.text;
            this.DelayResolution(resolve, {reason: "button", data });
          }
        }
      };
      DialogManager.container_node_.addEventListener("click", this.click_listener_);
      this.current_dialog_spec_ = spec;

    });

  }

}



