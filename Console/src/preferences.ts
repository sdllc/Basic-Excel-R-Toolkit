
const Constants = require("../data/constants.json");
const DefaultPreferences = require("../data/default_preferences.json");

// FIXME: export as static class member instead?
export const PreferencesSchema = require("../data/schemas/preferences_schema.json");

import * as Rx from 'rxjs';

export class Preferences {

  static PREFERENCES_KEY = "preferences"
  static PREFERENCES_URI = "localStorage://" + Preferences.PREFERENCES_KEY;

  /**  */
  private static events_:Rx.Subject<any> = new Rx.Subject<any>();

  /**  */
  public static get events() { return this.events_; }

  /** 
   * utility function: strip comments, c style and c++ style.
   * for json w/ comments
   */
  static StripComments(text:string) : string {
    return text.replace( /\/\*[\s\S]+\*\//g, "").split(/\n/).map( 
      line => line.replace( /\/\/.*$/m, "" )).join("\n");
  }

  /** 
   * loads preferences, either from storage or from defaults. 
   */
  static ReadPreferences() {

    let data = {};

    // if prefs does not exist, create from defaults and save first.
    // UPDATE: why save defaults? (...)

    let json = localStorage.getItem(Preferences.PREFERENCES_KEY);

    if (json) {
      try {
        data = JSON.parse(this.StripComments(json));
      }
      catch (e) {

        // FIXME: notify user, set defaults
        console.error(e);
      }
      return data;
    }

    data = DefaultPreferences;
    return data;

  }

}
