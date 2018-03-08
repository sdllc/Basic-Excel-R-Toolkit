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

import * as Rx from 'rxjs';
import * as chokidar from 'chokidar';

/** simple wrapper for chokidar, adds reference counting and an rx subject */
export class FileWatcher {

  private static reference_map_:{[index:string]:number} = {}; 

  private static events_:Rx.Subject<string> = new Rx.Subject<string>();

  public static get events() { return this.events_; }

  private static watcher_:any;

  public static IsWatching(file:string) : boolean {
    return !!this.reference_map_[file];
  }

  static Watch(file:string){
    if( this.reference_map_[file] ) this.reference_map_[file]++;
    else {
      this.reference_map_[file] = 1;
      if(!this.watcher_){
        this.watcher_ = chokidar.watch(file, {
          persistent: true
        });
        this.watcher_.on('change', (path, stats) => {
          this.events_.next(path);
        });
      }
      else this.watcher_.add(file);
    }
  }

  static Unwatch(file:string){
    if( !this.reference_map_[file] ) return;
    let count = --this.reference_map_[file];
    if(count) return;
    this.reference_map_[file] = 0;
    delete this.reference_map_[file]; // necessary?
    
  }

}