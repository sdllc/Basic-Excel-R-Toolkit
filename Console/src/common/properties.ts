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
import { ObservedProxy } from './observed_proxy';

export interface PropertyNotifcation {
  property: string;
  value: any;
}

/**
 * FIXME: like config, this class should (perhaps) inherit from
 * subject or behavior subject
 */
export class PropertyManager {

  /** notification */
  private change_ = new Rx.Subject<PropertyNotifcation>();

  /** accessor */
  public get change() { return this.change_; }

  /** opaque properties object */
  private properties_:any;
  
  /** accessor */
  public get properties(){ return this.properties_; }

  /**
   * we use a template to preset expected property structure, so that we 
   * can assume the existence of collections/objects. the proxy doesn't 
   * really support creating structure on the fly.
   */
  constructor(private storage_key_:string, template:any, read_existing = true){

    let base = {};
    let on_change:Function;

    if( storage_key_ ){
      if( read_existing ){
        let model = localStorage.getItem(storage_key_);
        if( model ){
          try {
            base = JSON.parse(model);
          }
          catch(e){
            console.error( "ERROR parsing storage model: " + e );
          }
        }
      }
      on_change = (property, value) => {
        this.change_.next({ property, value });
        localStorage.setItem(storage_key_, JSON.stringify(this.properties_));
      };
    }
    else {
      on_change = (property, value) => {
        this.change_.next({ property, value });
      };
    }

    if(template) this.ApplyTemplate(base, template);

    this.properties_ = ObservedProxy.Create(base, on_change);

  }

  ApplyTemplate(base:any, template:any){
    Object.keys(template).forEach( key => {
      if(!base.hasOwnProperty(key)){
        base[key] = JSON.parse(JSON.stringify(template[key])); // dumb deep copy // actually smart, see benchmarks
      }
      else {
        this.ApplyTemplate(base[key], template[key]);
      }
    });
  }

}
