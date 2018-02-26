
const Constants = require("../../data/constants.json");
const DefaultPreferences = require("../../data/default_preferences.json");

import { FileWatcher } from './file-watcher'; 

// FIXME: export as static class member instead?
export const PreferencesSchema = require("../../data/schemas/preferences.schema.json");

import * as fs from 'fs';
import * as path from 'path';
import * as Rx from 'rxjs';

/** 
 * no longer static, -> singleton 
 */
class PreferencesManager {

  /**  */
  private preferences_:Rx.Subject<any> = new Rx.Subject<any>();

  /**  */
  public get preferences() { return this.preferences_; }

  private preferences_path_ = path.join(
    process.env['BERT2_HOME_DIRECTORY'], "bert-config.json");

  public get preferences_path(){ return this.preferences_path_; }

  constructor(){

    // load the first time. that will generate an event clients can 
    // subscribe to. then start watching the file, so we get subsequent
    // changes. 

    this.ReadPreferences().then(prefs => {
      this.preferences_.next(prefs);
      FileWatcher.Watch(this.preferences_path);
      FileWatcher.events.filter( x => (x === this.preferences_path)).subscribe(x => {
        this.ReadPreferences().then(prefs => {
          this.preferences_.next(prefs);
        });
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
   * FIXME: coming from the old localstorage implementation,
   * this runs synchronously. make sure all the callers can 
   * handle async and then convert.
   */
  private ReadPreferences() : Promise<any> {
    return new Promise((resolve, reject) => {

      let prefs;
      let preferences_path = path.join(process.env['BERT2_HOME_DIRECTORY'], "bert-config.json");

      try {
        let json = fs.readFileSync(preferences_path, "utf8");
        prefs = JSON.parse(this.StripComments(json));
        return resolve(prefs);
      }
      catch (e) {
        console.error(e); // FIXME: notify user, set defaults
      }

      resolve(DefaultPreferences);
    });
  }
}

export const Preferences = new PreferencesManager()