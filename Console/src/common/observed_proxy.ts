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

export class ObservedProxy {

  static Create(object:any, callback:Function){
    const BuildProxy = function(object:any, prefix:string){
      return new Proxy(object, {
        set: function(target, property, value) {
          if( target[property] !== value ){
            target[property] = value;
            callback.call(null, prefix + property.toString(), value);
          }
          return true;
        },
        get: function(target, property) {
          let out = target[property];
          if (out instanceof Object && typeof property === "string") {
            return BuildProxy(out, prefix + property + '.');
          }
          return out;
        },
      });
    }
    return BuildProxy(object, "");  
  }

}
