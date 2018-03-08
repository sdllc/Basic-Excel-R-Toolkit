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

import * as less from 'less';
import * as path from 'path';
import * as fs from 'fs';
import { FileWatcher } from '../common/file_watcher'; 

export class UserStylesheet {

  private static stylesheet_path_ = path.join(
    process.env['BERT_HOME'], "user-stylesheet.less");

  public static get stylesheet_path() { return this.stylesheet_path_; }

  private static node:HTMLElement;

  static Attach(){

    this.node = document.createElement("style");
    this.node.setAttribute("type", "text/css");
    document.head.appendChild(this.node);

    this.Load().then(() => {

      // subscribe to changes
      FileWatcher.Watch(this.stylesheet_path_);
      FileWatcher.events.filter(x => x === this.stylesheet_path_).subscribe(x => {
        this.Load().then(() => {}).catch(err => {
          console.warn(err);
        });
      });

    }).catch(err => {
      console.warn(err);
    });
  }

  static Insert(){
    
  }

  static Load(){
    return new Promise((resolve, reject) => {
      fs.readFile(this.stylesheet_path_, "utf8", (err, data) => {
        if(err){
          return reject("user stylesheet not found");
        }
        less.render(data, {sourceMap:{}}).then(output => {
          this.node.innerText = output.css;
          resolve();
        }).catch(err => {
          return reject("less error: " + err);
        });
      });
    });
  }
}
