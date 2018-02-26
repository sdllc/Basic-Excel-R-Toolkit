
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