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

import * as messages from "../../generated/variable_pb.js";

// window['messages'] = messages;

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

        let arr = x.getArr();
        let rows = arr.getRows();
        let cols = arr.getCols();
        let list = arr.getDataList();

        // check for names. if no names, return an array
        let names = list.some(element => element.getName());

        // frame: colnames
        let colnames = x.getArr().getColnamesList();        
        if(colnames && colnames.length){
          let object = {colnames, rownames: arr.getRownamesList(), rows, cols, data: {}};
          let frame = this.GetFrame(arr);
          colnames.forEach((col, index) => object.data[col] = frame[index]);
          return object;
        }

        if( rows && cols && rows > 1 && cols > 1 ) {
          let src = arr.getDataList();          
          let data = new Array(rows);
          let index = 0;
          for( let row = 0; row< rows; row++ ){
            let row_data = new Array(cols);
            for( let col = 0; col< cols; col++ ){
              row_data[col] = this.VariableToObject(src[col * rows + row]);
            }
            data[row] = row_data;
          }
          return { rows, cols, data };
        }

        // return array [FIXME: matrix? ...]
        if (!names && (!colnames || !colnames.length)) return list.map(x => this.VariableToObject(x));

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

  /** get column-oriented 2d array */
  static GetFrame(arr){

    let nrows = arr.getRows();
    let ncols = arr.getCols();
    let data = arr.getDataList();
    let index = 0;

    let result = new Array(ncols);
    for( let c = 0; c< ncols; c++ ){
      result[c] = new Array(nrows);
      for( let r = 0; r< nrows; r++ ){
        result[c][r] = this.VariableToObject(data[index++]);
      }      
    }

    return result;
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