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

#pragma once

#include <iomanip>

/**
 * conversion utilities. converting between Excel/COM/PB types.
 *
 * note that Excel and R (the eventual target for these translations)
 * use different ordering in 2D array data, so we are shifting back
 * and forth. that might be different for different languages, so we
 * might think about moving that logic to the language-specific side
 * (in this case, controlR).
 */
class Convert {

public:

  /**
   * FIXME: this is a string function, move to string utilities.
   */
  static std::string WideStringToUtf8(const WCHAR *source, int len) {

    // FIXME: static buffer -- not thread safe. lock, use TLS 
    // storage, or allocate on each call

    static char *buffer = 0;
    static int buffer_length = 0;

    int u8_length = WideCharToMultiByte(CP_UTF8, 0, source, len, 0, 0, 0, 0);

    if (buffer_length < u8_length) {
      if (buffer) delete buffer;
      buffer_length = ((u8_length / 1024) + 1) * 1024;
      buffer = new char[buffer_length];
    }

    WideCharToMultiByte(CP_UTF8, 0, source, len, buffer, buffer_length, 0, 0);
    std::string u8(buffer, u8_length);
    return u8;

  }

  /** excel -> std::string */
  static std::string XLOPERToString(LPXLOPER12 x) {
    return WideStringToUtf8(&(x->val.str[1]), x->val.str[0]);
  }

  /** std::string -> excel */
  static void StringToXLOPER(LPXLOPER12 target, const std::string &source, bool flag_dll_free = true) {

    // FIXME: use a static (heap) buffer, and stop deleting it in the autofree routine

        // FIXME: absolutely not. there might be multiple allocated xlopers in flight at 
        // the same time. if you want more efficiency, think about using a slab allocator.

    int wide_char_count = MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1, 0, 0);

    // plus one for length. excel doesn't use the zero terminator, but when we call 
    // MBTWC again (below), it will expect space for it.
    WCHAR *wide_string = new WCHAR[wide_char_count + 1];

    // set length in WCHAR[0]
    wide_string[0] = wide_char_count - 1;

    // copy string starting at WCHAR[1]
    if (wide_char_count > 0) MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1, wide_string + 1, wide_char_count);

    target->xltype = xltypeStr;
    if (flag_dll_free) target->xltype |= xlbitDLLFree;
    target->val.str = wide_string;
  }

  /** com -> pb */
  static void VariantToVariable(BERTBuffers::Variable *variable, const CComVariant &variant) {

    switch (variant.vt) {
    case VT_CY:
      variable->set_real(variant.cyVal.int64/10000.0); // what happens in 32-bit excel?
      break;
    case VT_DATE:
      variable->set_real(variant.date);
      break;
    case VT_I4:
    case VT_I8:
      variable->set_integer(variant.intVal);
      break;
    case VT_R4:
    case VT_R8:
      variable->set_real(variant.dblVal);
      break;
    case VT_BOOL:
      variable->set_boolean(variant.boolVal);
      break;
    case VT_BSTR:
    {
      CComBSTR string_value(variant.bstrVal);
      variable->set_str(WideStringToUtf8(string_value.m_str, string_value.Length()));
      break;
    }

    case VT_ERROR:
      if (variant.scode == DISP_E_PARAMNOTFOUND) {
        variable->set_missing(true);
      }
      else {
        variable->mutable_err()->set_message("COM error");
      }
      break;
   
    //case VT_DISPATCH: // handled separately. FIXME: merge? (...maybe)

    default:

      if (variant.vt & VT_ARRAY) {
        SafeArrayToVariable(variant, variable);
      }
      else {
        if (variant.vt & VT_BYREF) std::cout << " ** unhandled VT (BYREF): " << (variant.vt&(~VT_BYREF)) << std::endl;
        else std::cout << " ** unhandled VT: " << variant.vt << std::endl;
        variable->set_missing(true);
      }

    }

  }

  static void SafeArrayToVariable(const CComVariant &variant, BERTBuffers::Variable *variable) {

    VARTYPE vartype = variant.vt & (~VT_ARRAY);
    int rows = 1, cols = 1;

    UINT dimensions = SafeArrayGetDim(variant.parray);
    if (dimensions == 1) {

      long lbound, ubound;
      SafeArrayGetLBound(variant.parray, 1, &lbound);
      SafeArrayGetUBound(variant.parray, 1, &ubound);
      rows = ubound - lbound + 1;

    }
    else if (dimensions == 2) {

      long lbound, ubound;
      SafeArrayGetLBound(variant.parray, 1, &lbound);
      SafeArrayGetUBound(variant.parray, 1, &ubound);
      rows = ubound - lbound + 1;

      SafeArrayGetLBound(variant.parray, 2, &lbound);
      SafeArrayGetUBound(variant.parray, 2, &ubound);
      cols = ubound - lbound + 1;

    }
    else { // >2 not handled
      variable->mutable_err()->set_message("array of >2 dimensions not handled");
      return;
    }

    auto array_data = variable->mutable_arr();
    array_data->set_rows(rows);
    array_data->set_cols(cols);

    int length = rows*cols;

    // access is typed

    void *data_pointer;
    SafeArrayAccessData(variant.parray, &data_pointer);

    switch (vartype) {
    case VT_BSTR:
    {
      BSTR *pv = reinterpret_cast<BSTR*>(data_pointer);
      for (int i = 0; i < length; i++)
      {
        CComBSTR bstr(pv[i]);
        array_data->add_data()->set_str(WideStringToUtf8(bstr.m_str, bstr.Length()));
      }
      break;
    }
    case VT_INT:
    case VT_I4:
    case VT_I8:
    {
      int *pv = reinterpret_cast<int*>(data_pointer);
      for (int i = 0; i < length; i++)
      {
        array_data->add_data()->set_integer(pv[i]);
      }
      break;
    }
    case VT_R4:
    case VT_R8:
    {
      double *pv = reinterpret_cast<double*>(data_pointer);
      for (int i = 0; i < length; i++)
      {
        array_data->add_data()->set_real(pv[i]);
      }
      break;
    }
    case VT_BOOL:
    {
      VARIANT_BOOL *pv = reinterpret_cast<VARIANT_BOOL*>(data_pointer);
      for (int i = 0; i < length; i++)
      {
        array_data->add_data()->set_boolean(pv[i]);
      }
      break;
    }
    case VT_VARIANT:
    {
      VARIANT *pv = reinterpret_cast<VARIANT*>(data_pointer);
      for (int i = 0; i < length; i++)
      {
        VariantToVariable(array_data->add_data(), pv[i]);
      }
      break;
    }
    default:
      std::cout << " ** unhandled array type: " << vartype << std::endl;
    }
    
    SafeArrayUnaccessData(variant.parray);
    
  }

  /** pb -> com */
  static CComVariant VariableToVariant(const BERTBuffers::Variable &var) {

    // FIXME: array, factor, (frame?), ...

    CComVariant variant;

    switch (var.value_case()) {
    case BERTBuffers::Variable::ValueCase::kBoolean:
      variant = var.boolean();
      break;
    //case BERTBuffers::Variable::ValueCase::kNum:
    //  variant = var.num();
    //  break;
    case BERTBuffers::Variable::ValueCase::kReal:
      variant = var.real();
      break;
    case BERTBuffers::Variable::ValueCase::kInteger:
      variant = var.integer();
      break;
    case BERTBuffers::Variable::ValueCase::kStr:
      variant = var.str().c_str();
      break;
    case BERTBuffers::Variable::ValueCase::kNil:
      variant.vt = VT_NULL;
      break;
    case BERTBuffers::Variable::ValueCase::kMissing:
      variant.vt = VT_ERROR;
      variant.scode = DISP_E_PARAMNOTFOUND;
      break;
    case BERTBuffers::Variable::ValueCase::kArr:
    {
      auto arr = var.arr();
      int rows = arr.rows();
      int cols = arr.cols();
      int length = arr.data_size();

      // ensure there's data. if not, treat as missing.

      if (arr.data_size() == 0) {
        variant.vt = VT_ERROR;
        variant.scode = DISP_E_PARAMNOTFOUND;
      }
      else {

        int count = rows * cols;

        bool col_names = (cols && arr.colnames_size() == cols);
        bool row_names = (rows && arr.rownames_size() == rows);

        if (length > 0 && count == 0) {
          count = length;
          cols = length;
          rows = 1;
        }
        else if (count > length) {
          variant.vt = VT_ERROR;
          break;
        }

        /*
        if (cols == 0 && rows == 0) {
          cols = 1;
          rows = arr.data_size();
        }
        else if (cols == 0 && rows > 0) cols = 1;
        else if (rows == 0 && cols > 0) rows = 1;
        */

        if (col_names) rows++;
        if (row_names) cols++;

        SAFEARRAYBOUND array_bounds[2];

        array_bounds[0].cElements = rows;
        array_bounds[0].lLbound = 0;
        array_bounds[1].cElements = cols;
        array_bounds[1].lLbound = 0;

        CComSafeArray<VARIANT> variant_array(array_bounds, 2);

        int32_t index = 0;
        LONG indexes[2]; // is this type architecture-dependent? otherwise it should be === int32_t, no?

        int c_offset = (row_names ? 1 : 0);
        int r_offset = (col_names ? 1 : 0);

        /*
        for (int col = 0; col < cols; col++) {
          indexes[1] = col;
          for (int row = 0; row < rows; row++) {
            indexes[0] = row;
            variant_array.MultiDimSetAt(indexes, VariableToVariant(arr.data(index++)));
          }
        }
        */

        for (int c = 0; c < cols; c++) {
          indexes[1] = c;
          if (row_names && c == 0) {
            if (col_names) //StringToXLOPER(&(x->val.array.lparray[0]), "");
              variant_array.MultiDimSetAt(indexes, CComVariant(CComBSTR("")));
            for (int r = r_offset; r < rows; r++) {
              //StringToXLOPER(&(x->val.array.lparray[r * cols + c]), arr.rownames(r - r_offset));
              variant_array.MultiDimSetAt(indexes, CComVariant(CComBSTR(arr.rownames(r - r_offset).c_str())));
            }
          }
          else {
            for (int r = 0; r < rows; r++) {
              indexes[0] = r;
              if (col_names && r == 0) {
                //StringToXLOPER(&(x->val.array.lparray[r * cols + c]), arr.colnames(c - c_offset));
                variant_array.MultiDimSetAt(indexes, CComVariant(CComBSTR(arr.colnames(c - c_offset).c_str())));
              }
              else {
                //VariableToXLOPER(&(x->val.array.lparray[r * cols + c]), arr.data(index++));
                variant_array.MultiDimSetAt(indexes, VariableToVariant(arr.data(index++)));
              }
            }
          }
        }

        return CComVariant(variant_array);
      }
    }

    case BERTBuffers::Variable::ValueCase::kErr:
      variant.vt = VT_ERROR;
      break;
    }

    return variant;
  }

  /** pb -> excel */
  static LPXLOPER12 VariableToXLOPER(LPXLOPER12 x, const BERTBuffers::Variable &var) {

    switch (var.value_case()) {
    case BERTBuffers::Variable::ValueCase::kBoolean:
      x->xltype = xltypeBool;
      x->val.xbool = var.boolean();
      break;
    
    case BERTBuffers::Variable::ValueCase::kInteger:
      x->xltype = xltypeInt;
      x->val.w = var.integer();
      break;
    
    case BERTBuffers::Variable::ValueCase::kReal:
      x->xltype = xltypeNum;
      x->val.num = var.real();
      break;
    
    case BERTBuffers::Variable::ValueCase::kStr:
      StringToXLOPER(x, var.str());
      break;

    case BERTBuffers::Variable::ValueCase::kCacheReference:
    {
      std::stringstream ss;
      ss << "{OBJECT:" << std::hex << var.cache_reference() << "}";
      StringToXLOPER(x, ss.str());
      break;
    }

    case BERTBuffers::Variable::ValueCase::kCpx:
    {
      std::stringstream ss;
      auto complex = var.cpx();
      ss << complex.r();
      if (complex.i() >= 0) ss << "+";
      ss << complex.i();
      ss << "i"; // FIXME: customizable
      StringToXLOPER(x, ss.str());
      break;
    }

    case BERTBuffers::Variable::ValueCase::kArr:
    {
      const BERTBuffers::Array &arr = var.arr();
      int rows = arr.rows();
      int cols = arr.cols();
      int count = rows * cols;
      int len = arr.data_size();

      bool col_names = (cols && arr.colnames_size() == cols);
      bool row_names = (rows && arr.rownames_size() == rows);

      if (len > 0 && count == 0) {
        count = len;
        cols = len;
        rows = 1;
      }
      else if (count > len) {
        x->xltype = xltypeErr;
        x->val.err = xlerrValue;
        std::cerr << "ERROR: invalid count/length" << std::endl;
        return x;
      }

      if (count > 0) {

        if (col_names) rows++;
        if (row_names) cols++;

        x->xltype = xltypeMulti | xlbitDLLFree;
        x->val.array.columns = cols;
        x->val.array.rows = rows;
        x->val.array.lparray = new XLOPER12[rows * cols];

        int c_offset = (row_names ? 1 : 0);
        int r_offset = (col_names ? 1 : 0);

        int index = 0;
        for (int c = 0; c < cols; c++) {
          if (row_names && c == 0) {
            if (col_names) StringToXLOPER(&(x->val.array.lparray[0]), "");
            for (int r = r_offset; r < rows; r++) {
              StringToXLOPER(&(x->val.array.lparray[r * cols + c]), arr.rownames(r - r_offset));
            }
          }
          else {
            for (int r = 0; r < rows; r++) {
              if (col_names && r == 0) {
                StringToXLOPER(&(x->val.array.lparray[r * cols + c]), arr.colnames(c - c_offset));
              }
              else {
                VariableToXLOPER(&(x->val.array.lparray[r * cols + c]), arr.data(index++));
              }
            }
          }
        }

      }
      else {
        x->xltype = xltypeErr;
        x->val.err = xlerrValue;
      }

      break;
    }

    case BERTBuffers::Variable::ValueCase::kErr:
      x->xltype = xltypeErr;
      if (var.err().type() == BERTBuffers::ErrorType::NA) x->val.err = xlerrNA;
      else x->val.err = xlerrValue;
      break;

    case BERTBuffers::Variable::ValueCase::kNil:
      x->xltype = xltypeNil;
      break;

    case BERTBuffers::Variable::ValueCase::kRef:
    {
      auto reference = var.ref();
      uint64_t sheet_id = reference.sheet_id();
      if (sheet_id) {

        // sheet id, use mref

        x->xltype = xltypeRef | xlbitXLFree;
        x->val.mref.idSheet = sheet_id;
        x->val.mref.lpmref = new XLMREF12;
        x->val.mref.lpmref->count = 1;
        x->val.mref.lpmref->reftbl[0].rwFirst = reference.start_row() - 1;
        x->val.mref.lpmref->reftbl[0].rwLast = reference.end_row() - 1;
        x->val.mref.lpmref->reftbl[0].colFirst = reference.start_column() - 1;
        x->val.mref.lpmref->reftbl[0].colLast = reference.end_column() - 1;

      }
      else {

        // no sheet id, use sref type

        x->xltype = xltypeSRef;
        x->val.sref.count = 1; 
        x->val.sref.ref.rwFirst = reference.start_row() - 1;
        x->val.sref.ref.rwLast = reference.end_row() - 1;
        x->val.sref.ref.colFirst = reference.start_column() - 1;
        x->val.sref.ref.colLast = reference.end_column() - 1;

      }

      break;
    }

    case BERTBuffers::Variable::ValueCase::VALUE_NOT_SET:
    default:
      x->xltype = xltypeErr;
      x->val.err = xlerrNA;
      break;

    }

    return x; // fluent
  }

  /** excel -> pb */
  static BERTBuffers::Variable * XLOPERToVariable(BERTBuffers::Variable *var, LPXLOPER12 x) {

    if (x->xltype & xltypeStr) {
      if (x->val.str[0] > 9 && !wcsncmp(x->val.str + 1, L"{OBJECT:", 8)) {
        std::wistringstream text(x->val.str + 9);
        uint32_t value;
        text >> std::hex >> value;
        var->set_cache_reference(value);
      }
      else var->set_str(XLOPERToString(x));
    }
    else if (x->xltype & xltypeNum) {
      var->set_real(x->val.num);
    }
    else if (x->xltype & xltypeInt) {
      var->set_integer(x->val.w);
    }
    else if (x->xltype & xltypeBool) {
      var->set_boolean(x->val.xbool ? true : false);
    }
    else if (x->xltype & xltypeMissing) {
      //var->set_nil(true);
      var->set_missing(true);
    }
    else if (x->xltype & xltypeNil) {
      var->set_nil(true);
      //var->set_missing(true);
    }
    else if (x->xltype & xltypeMulti) {

      int cols = x->val.array.columns;
      int rows = x->val.array.rows;

      auto arr = var->mutable_arr();
      arr->set_cols(cols);
      arr->set_rows(rows);

      for (int c = 0; c < cols; c++) {
        for (int r = 0; r < rows; r++) {
          XLOPERToVariable(arr->add_data(), &(x->val.array.lparray[r * cols + c]));
        }
      }

    }
    else if ((x->xltype & xltypeSRef) || (x->xltype & xltypeRef)) {

      // we don't expect SRef from a function call, because we use the Q type 
      // which converts automatically. however we may see this from an Excel 
      // API call, so we still need to handle it. in that particular case, we 
      // want to preserve it as a reference.

      auto reference = var->mutable_ref();

      LPXLREF12 reference_pointer = 0;

      if (x->xltype & xltypeRef)
      {
        reference->set_sheet_id(x->val.mref.idSheet);
        if (x->val.mref.lpmref && x->val.mref.lpmref->count > 0) reference_pointer = &(x->val.mref.lpmref->reftbl[0]);
      }
      else // sref
      {
        reference_pointer = &(x->val.sref.ref);
      }

      if (reference_pointer)
      {
        reference->set_start_row(reference_pointer->rwFirst + 1);
        reference->set_end_row(reference_pointer->rwLast + 1);
        reference->set_start_column(reference_pointer->colFirst + 1);
        reference->set_end_column(reference_pointer->colLast + 1);
      }

    }
    else {
      std::stringstream ss;
      ss << "unexpected type: " << x->xltype;
      auto err = var->mutable_err();
      err->set_type(BERTBuffers::ErrorType::GENERIC);
      err->set_message(ss.str());
    }

    return var; // fluent
  }

};
