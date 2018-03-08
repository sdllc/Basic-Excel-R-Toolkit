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


const PREFIX = "cache_";
const VERSION = 2;
const DEFAULT_EXPIRATION = (60 * 60 * 24);

export class DataCache {
 
  static Store(key:string, data:any, expiration=DEFAULT_EXPIRATION){
    let expireTime = (new Date().getTime() / 1000) + expiration;
    localStorage.setItem(PREFIX + key, JSON.stringify({
      version:VERSION, expiration:expireTime, data
    }));
  }

  static Check(key:string) : DataCache.CacheStatus {
    return this.Get(key).status;
  }

  static Flush(key:string) {
    localStorage.removeItem(PREFIX + key);
  }

  static Get(key:string) : DataCache.CacheResult {
    let json = localStorage.getItem(PREFIX + key);
    let time = (new Date().getTime() / 1000);
    if(!json) return {status: DataCache.CacheStatus.NotFound};
    try {
      let cache = JSON.parse(json) as DataCache.CacheData;
      let status = (time < cache.expiration) ? DataCache.CacheStatus.Valid : DataCache.CacheStatus.Expired;
      return {status, data:cache.data};
    }
    catch(e){
      console.error(e);
      return {status:DataCache.CacheStatus.Error};
    }
  }

}

export namespace DataCache {

  export enum CacheStatus {
    NotFound, 
    Expired, 
    Valid,
    Error
  }
  
  export interface CacheResult {
    status:CacheStatus;
    data?:any;
  }
  
  export interface CacheData {
    version:number,
    expiration:number,
    data:any
  }
  
}
