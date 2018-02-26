
import * as messages from "../../generated/variable_pb.js";

window['messages'] = messages;

export class MessageUtilities {
  
  /** Variable (protobuf type) to js object */
  static VariableToObject(x) {

    switch (x.getValueCase()) {
      case messages.Variable.ValueCase.NIL:
        return null;
      case messages.Variable.ValueCase.INTEGER:
        return x.getInteger();
      case messages.Variable.ValueCase.REAL:
        return x.getReal();
      case messages.Variable.ValueCase.STR:
        return x.getStr();
      case messages.Variable.ValueCase.BOOLEAN:
        return x.getBoolean();
      case messages.Variable.ValueCase.ARR:

        let list = x.getArr().getDataList();

        // check for names. if no names, return an array
        let names = list.some(element => element.getName());
        if (!names) return list.map(x => this.VariableToObject(x));

        // if names, return an object. there may be unnamed items, use $X
        let object = {};
        list.forEach((entry, i) => {
          object[entry.getName() || `$${i}`] = this.VariableToObject(entry);
        });
        return object;

      default:
        console.info(`UNTRANSLATED (${x.getValueCase()})\n`, x.toObject());
        return null;
    }
  }

  /** js intrinsic or object to protobuf Variable */
  static ObjectToVariable(x) {
    let type = typeof (x);

    let v = new messages.Variable();
    if (null === x) v.setNil(true);
    else if( Array.isArray(x)){
      let arr = new messages.Array;
      arr.setDataList(x.map(element => this.ObjectToVariable(element)));
      v.setArr(arr);
    }
    else {
      switch (type) {
        case "number":
          v.setReal(x);
          break;
        case "string":
          v.setStr(x);
          break;
        case "boolean":
          v.setBoolean(x);
          break;
        default:
          console.warn("unexpected type...", type );
      }
    }
    return v;
  }

}