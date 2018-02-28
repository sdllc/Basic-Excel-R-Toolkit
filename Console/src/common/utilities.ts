
import { Base64 } from 'js-base64';

export class Utilities {

  /**
   * thanks to
   * https://stackoverflow.com/questions/12710001/how-to-convert-uint8-array-to-base64-encoded-string
   */
  static Uint8ToBase64(data:Uint8Array):string{

    let chunks = [];
    let block = 0x8000;
    for( let i = 0; i< data.length; i += block){
      chunks.push( String.fromCharCode.apply(null, data.subarray(i, i + block)));
    }
    return Base64.encode(chunks.join(""));

  }

  static VersionToNumber(version_string){
    let version = 0;
    if(version_string){
      version_string.split(/\./).forEach(component => {
        version *= 1000;
        version += Number(component||0)
      });
    }
    return version;
  }

}

