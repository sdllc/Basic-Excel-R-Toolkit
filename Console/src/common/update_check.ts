
import {AlertManager, AlertResult, AlertSpec} from '../ui/alert';
import {shell as electron_shell} from 'electron';

const Constants = require("../../data/constants.json");

export namespace UpdateCheck {

  /**
   * checks for updates. if the current version > this version, alerts
   * the user. the user can say "don't notify me about this version again",
   * in which case we will toll notification until the _next_ update. 
   *
   * fixme: allow disable?
   */
  export async function CheckForUpdates(prefs, alert_manager:AlertManager){

    // there's a chrome bug (not a bug) that prevents setting the UA 
    // header, but electron is helpfully adding our process name/version 
    // anyway.

    // (as I recall we can set headers using old-style XHRs, if necessary)
    
    /*
    let headers = new Headers({
      "User-Agent": "BERT-Console-" + process.env.BERT_VERSION
    });
    */

    let response = await fetch("https://bert-toolkit.com/version.json", {cache: "no-cache"});
    let version_object = await response.json();

    let version_number = 
      version_object.version.split(".").reduce((a,x) => a * 1000 + Number(x), 0);

    let current_version = 
      process.env.BERT_VERSION.split(".").reduce((a,x) => a * 1000 + Number(x), 0);

    // version is up-to-date (or newer)
    if( current_version >= version_number ) return; 

    // already notified
    let suppress_version = prefs.console.suppress_version || 0;
    if( suppress_version === version_number ) return;

    alert_manager.Show({
      escape: true, enter: true,
      title: Constants.updates.title.replace(/\$VERSION/, version_object.version),
      message: `<button data-result='download'>${Constants.updates.downloadLink}</button> <button data-result='suppress'>${Constants.updates.suppressLink}</button>`
    }).then(result => {
      switch(result.data){
      case "suppress":
        prefs.console.suppress_version = version_number;
        break;
      case "download":
        electron_shell.openExternal(version_object.link);
        break;
      }
    });
    
  }

}