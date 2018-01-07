
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
