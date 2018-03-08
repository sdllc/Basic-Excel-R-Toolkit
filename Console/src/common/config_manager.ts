
const Constants = require("../../data/constants.json");
const DefaultConfig = require("../../data/default_config.json");

import { FileWatcher } from './file_watcher'; 

// FIXME: export as static class member instead?
export const ConfigSchema = require("../../data/schemas/config.schema.json");

import * as fs from 'fs';
import * as path from 'path';
import * as Rx from 'rxjs';

export enum ConfigLoadStatus {
  NotLoaded = 0,
  Loaded = 1,
  Error = 2
}

export interface ConfigState {
  config?:any;
  status:ConfigLoadStatus;
}

/** 
 * no longer static, -> singleton 
 */
class ConfigManager extends Rx.BehaviorSubject<ConfigState> {

  /**  */
  public get config() : any { 
    return this.getValue().config;
  }

  /** */
  public get status() : ConfigLoadStatus { 
    return this.getValue().status;
  }

  private config_path_ = path.join(
    process.env['BERT_HOME'], "bert-config.json");

  public get config_path(){ return this.config_path_; }

  constructor(){
    
    super({ status:ConfigLoadStatus.NotLoaded });

    // load the first time. that will generate an event clients can 
    // subscribe to. then start watching the file, so we get subsequent
    // changes. 

    this.ReadConfig().then(() => {
      FileWatcher.Watch(this.config_path);
      FileWatcher.events.filter( x => (x === this.config_path)).subscribe(x => {
       this.ReadConfig();
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
  private ReadConfig() : Promise<any> {
    return new Promise((resolve, reject) => {
      fs.readFile(this.config_path, "utf8", (err, json) => {
        if(!err){
          try {
            let config = JSON.parse(this.StripComments(json));
            this.next({ config, status:ConfigLoadStatus.Loaded });
            return resolve();
          }
          catch(e){
            console.error(e); 
          }
        }
        else {
          console.error("err loading config file: " + err);
        }
        this.next({
          config:DefaultConfig, status:ConfigLoadStatus.Error
        });
        return resolve();
      });
    });
  }
}

export const ConfigManagerInstance = new ConfigManager();
