
export class Utilities {

  /**
   * thanks to
   * https://stackoverflow.com/questions/12710001/how-to-convert-uint8-array-to-base64-encoded-string
   *
   * FIXME: move to utility library
   */
  static Uint8ToBase64(data:Uint8Array):string{

    let chunks = [];
    let block = 0x8000;
    for( let i = 0; i< data.length; i += block){
      chunks.push( String.fromCharCode.apply(null, data.subarray(i, i + block)));
    }
    return btoa(chunks.join(""));

  }

}

