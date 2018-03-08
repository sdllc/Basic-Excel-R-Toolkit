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

  private raw_config_file_:string;

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
   * operates on a commented json file. insert should be pre-rendered 
   * json. we can't handle arrays, just objects.
   */
  InsertJSON(path:string, json:string){
    let source = this.raw_config_file_;
    let components = path.split(".");

    
    
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
          this.raw_config_file_ = json;
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
        this.raw_config_file_ = DefaultConfig;
        this.next({
          config:DefaultConfig, status:ConfigLoadStatus.Error
        });
        return resolve();
      });
    });
  }
}

export const ConfigManagerInstance = new ConfigManager();
