
const Constants = require("../../data/constants.json");
const DefaultPreferences = require("../../data/default_preferences.json");

import { FileWatcher } from './file-watcher'; 

// FIXME: export as static class member instead?
export const PreferencesSchema = require("../../data/schemas/preferences.schema.json");

import * as fs from 'fs';
import * as path from 'path';
import * as Rx from 'rxjs';

export enum PreferencesLoadStatus {
  NotLoaded = 0,
  Loaded = 1,
  Error = 2
}

export interface PreferencesState {
  preferences?:any;
  status:PreferencesLoadStatus;
}

/** 
 * no longer static, -> singleton 
 */
class PreferencesManager extends Rx.BehaviorSubject<PreferencesState> {

  /**  
   * we're now using a composite event state that indicates any 
   * load errors as well as the data. so don't switch on null
   * anymore, switch on status. 
   * /
  private subject_:Rx.BehaviorSubject<PreferencesState> = new Rx.BehaviorSubject<PreferencesState>({ 
    status:PreferencesLoadStatus.NotLoaded 
  });
  */

  /**  */
  public get preferences() : any { 
    return this.getValue().preferences;
  }

  /** */
  public get status() : PreferencesLoadStatus { 
    return this.getValue().status;
  }

  /** * /
  public subscribe(observer) : Rx.Subscription {
    return this.subject_.subscribe(observer);
  }
  */

  private preferences_path_ = path.join(
    process.env['BERT2_HOME_DIRECTORY'], "bert-config.json");

  public get preferences_path(){ return this.preferences_path_; }

  constructor(){
    
    super({ status:PreferencesLoadStatus.NotLoaded });

    // load the first time. that will generate an event clients can 
    // subscribe to. then start watching the file, so we get subsequent
    // changes. 

    this.ReadPreferences().then(() => {
      FileWatcher.Watch(this.preferences_path);
      FileWatcher.events.filter( x => (x === this.preferences_path)).subscribe(x => {
       this.ReadPreferences();
      });
    });

  }

  /** 
   * utility function: strip comments, c style and c++ style.
   * for json w/ comments
   */
  StripComments(text:string) : string {
    return text.replace( /\/\*[\s\S]+?\*\//g, "").split(/\n/).map( 
      line => line.replace( /\/\/.*$/m, "" )).join("\n");
  }

  /** 
   * 
   */
  private ReadPreferences() : Promise<any> {
    return new Promise((resolve, reject) => {
      fs.readFile(this.preferences_path, "utf8", (err, json) => {
        if(!err){
          try {
            let preferences = JSON.parse(this.StripComments(json));
            this.next({ preferences, status:PreferencesLoadStatus.Loaded });
            return resolve();
          }
          catch(e){
            console.error(e); 
          }
        }
        else {
          console.error("err loading preferences file: " + err);
        }
        this.next({
          preferences:DefaultPreferences, status:PreferencesLoadStatus.Error
        });
        return resolve();
      });
    });
  }
}

export const Preferences = new PreferencesManager();
