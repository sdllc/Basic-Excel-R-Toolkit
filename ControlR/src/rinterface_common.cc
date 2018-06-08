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

#include "controlr.h"
#include "controlr_common.h"
#include "console_graphics_device.h"
#include "spreadsheet_graphics_device.h"
#include "convert.h"

#include "gdi_graphics_device.h"

// try to store fuel now, you jerks
#undef clear
#undef length

SEXP ResolveCacheReference(uint32_t reference);

void ReleaseExternalPointer(SEXP external_pointer) {
  RCallback(Rf_mkString("release-pointer"), external_pointer);
}

void SetNames(SEXP variable, const std::vector<std::string> &names) {
  int len = (int)names.size();
  SEXP names_sexp = Rf_allocVector(STRSXP, len);
  for (int i = 0; i < len; i++) {
    SET_STRING_ELT(names_sexp, i, Rf_mkChar(names[i].c_str()));
  }
  Rf_setAttrib(variable, R_NamesSymbol, names_sexp);
}

SEXP VariableToSEXP(const BERTBuffers::Variable &var) {

  switch (var.value_case()) {
  case BERTBuffers::Variable::ValueCase::kNil:
  case BERTBuffers::Variable::ValueCase::kMissing:
    return R_NilValue;

  case BERTBuffers::Variable::ValueCase::kStr:
    return Rf_mkString(var.str().c_str());

  case BERTBuffers::Variable::ValueCase::kInteger:
    return Rf_ScalarInteger(var.integer());

  case BERTBuffers::Variable::ValueCase::kReal:
    return Rf_ScalarReal(var.real());

  case BERTBuffers::Variable::ValueCase::kBoolean:
    return Rf_ScalarLogical(var.boolean() ? 1 : 0);

  case BERTBuffers::Variable::ValueCase::kCpx:
    return Rf_ScalarComplex(Rcomplex{ var.cpx().r(), var.cpx().i() });

  case BERTBuffers::Variable::ValueCase::kRef:
  {
    // this assumes we have the xlReference type, which is in our 
    // support module. it should fail if that's not loaded.

    SEXP object = R_do_new_object(R_getClassDef("xlReference"));
    if (object) {
      R_do_slot_assign(object, Rf_mkString("R1"), Rf_ScalarInteger(var.ref().start_row()));
      R_do_slot_assign(object, Rf_mkString("C1"), Rf_ScalarInteger(var.ref().start_column()));
      R_do_slot_assign(object, Rf_mkString("R2"), Rf_ScalarInteger(var.ref().end_row()));
      R_do_slot_assign(object, Rf_mkString("C2"), Rf_ScalarInteger(var.ref().end_column()));
      uint64_t sheet_id = var.ref().sheet_id();
      R_do_slot_assign(object, Rf_mkString("SheetID"), Rf_list2(ScalarInteger((INT32)(sheet_id >> 32)), ScalarInteger((INT32)(sheet_id & 0xffffffff))));
    }

    // FIXME: err?

    return object;
  }

  case BERTBuffers::Variable::ValueCase::kArr:
  {
    const BERTBuffers::Array &arr = var.arr();

    int rows = arr.rows();
    int cols = arr.cols();
    int count = rows * cols;

    // if there are now rows/cols, use data_size() and treat as rows (for no reason).
    // do this as well if there's a mismatch.

    if (!count && arr.data_size()
      || count != arr.data_size()) {
      count = rows = arr.data_size();
      cols = 0;
    }
    
    // check for single type (in R, we can include nulls and NAs in the array)

    MessageUtilities::TypeFlags type_flags = MessageUtilities::CheckArrayType(arr, true, true);

    bool has_names = false;

    for (int i = 0; i < count && !has_names; i++) {
      has_names = has_names || arr.data(i).name().length();
    }

    // FIXME: can merge int and logical? (...)
    // FIXME: can we use the coercion functions (asReal &c) here?  would simplify

    SEXP list;
    if (type_flags & MessageUtilities::TypeFlags::numeric) {
      if (type_flags & MessageUtilities::TypeFlags::integer) {
        if (!cols) list = Rf_allocVector(INTSXP, count);
        else list = Rf_allocMatrix(INTSXP, rows, cols);
        PROTECT(list);
        
        int *p = INTEGER(list);
        for (int i = 0; i < count; i++) {
          auto value_case = arr.data(i).value_case();
          if((value_case == BERTBuffers::Variable::ValueCase::kNil) || (value_case == BERTBuffers::Variable::ValueCase::kMissing)) p[i] = NA_INTEGER;
          else p[i] = arr.data(i).integer();
        }
      }
      else {
        if (!cols) list = Rf_allocVector(REALSXP, count);
        else list = Rf_allocMatrix(REALSXP, rows, cols);
        PROTECT(list);

        double *p = REAL(list);
        for (int i = 0; i < count; i++) {
          auto value_case = arr.data(i).value_case();
          if ((value_case == BERTBuffers::Variable::ValueCase::kNil) || (value_case == BERTBuffers::Variable::ValueCase::kMissing)) p[i] = NA_REAL;
          else if( value_case == BERTBuffers::Variable::ValueCase::kInteger) p[i] = arr.data(i).integer();
          else p[i] = arr.data(i).real();
        }
      }
    }
    else if (type_flags & MessageUtilities::TypeFlags::logical) {
      if (!cols) list = Rf_allocVector(LGLSXP, count);
      else list = Rf_allocMatrix(LGLSXP, rows, cols);
      PROTECT(list);

      int *p = LOGICAL(list);
      for (int i = 0; i < count; i++) {
        auto value_case = arr.data(i).value_case();
        if ((value_case == BERTBuffers::Variable::ValueCase::kNil) || (value_case == BERTBuffers::Variable::ValueCase::kMissing)) p[i] = NA_LOGICAL;
        else p[i] = arr.data(i).boolean() ? -1 : 0;
      }
    }
    else if (type_flags & MessageUtilities::TypeFlags::string) {
      if (!cols) list = Rf_allocVector(STRSXP, count);
      else list = Rf_allocMatrix(STRSXP, rows, cols);
      PROTECT(list);

      for (int i = 0; i < count; i++) {
        auto value_case = arr.data(i).value_case();
        if ((value_case == BERTBuffers::Variable::ValueCase::kNil) || (value_case == BERTBuffers::Variable::ValueCase::kMissing)) {
          SET_STRING_ELT(list, i, NA_STRING);
        }
        else {
          SET_STRING_ELT(list, i, Rf_mkChar(arr.data(i).str().c_str()));
        }
      }
    }
    else {
      if (!cols) list = Rf_allocVector(VECSXP, count);
      else list = Rf_allocMatrix(VECSXP, rows, cols);
      PROTECT(list);

      for (int i = 0; i < count; i++) {
        SEXP elt = SET_VECTOR_ELT(list, i, VariableToSEXP(arr.data(i)));
      }
    }

    if (has_names) {
      SEXP names = Rf_allocVector(VECSXP, count);
      for (int i = 0; i < count; i++) {
        if (arr.data(i).name().length()) SET_VECTOR_ELT(names, i, Rf_mkString(arr.data(i).name().c_str()));
      }
      Rf_setAttrib(list, R_NamesSymbol, names);
    }

    UNPROTECT(1);

    return list;
  }
  case BERTBuffers::Variable::ValueCase::kComPointer:
  {
    // create a structure for this that we can pass to R. we could do the 
    // actual creation here or we can just create a descriptor and let R
    // do the work. (do that).

    const auto &com_pointer = var.com_pointer();
    std::cout << "installing external pointer: " << com_pointer.interface_name() << " @ " << std::hex << com_pointer.pointer() << std::endl;

    SEXP external_pointer = R_MakeExternalPtr((void*)(var.com_pointer().pointer()), install("COM dispatch pointer"), R_NilValue);
    R_RegisterCFinalizerEx(external_pointer, (R_CFinalizer_t)ReleaseExternalPointer, TRUE);

    SEXP descriptor = Rf_allocVector(VECSXP, 4);
    SET_VECTOR_ELT(descriptor, 0, Rf_mkString(com_pointer.interface_name().c_str()));
    SET_VECTOR_ELT(descriptor, 1, external_pointer);

    // there's a possibility (in fact a good likelihood) of repeated names,
    // where two accessors have the same name. 

    int len = com_pointer.functions_size();
    SEXP functions_list = Rf_allocVector(VECSXP, len);
    std::vector< std::string > function_names;
    for (int i = 0; i < len; i++) {

      auto com_function = com_pointer.functions(i);
      std::string call_type = "method";
      std::string function_name = "";
      switch (com_function.call_type()) {
      case BERTBuffers::CallType::get:
        call_type = "get";
        function_name = call_type;
        function_name += "_";
        break;
      case BERTBuffers::CallType::put:
        call_type = "put";
        function_name = call_type;
        function_name += "_";
        break;
      }
      function_name += com_function.function().name();
      function_names.push_back(function_name);

      // for now, arguments are just names
      int arguments_len = com_function.arguments_size();
      SEXP r_arguments_list = Rf_allocVector(STRSXP, arguments_len);
      for (int j = 0; j < arguments_len; j++) {
        SET_STRING_ELT(r_arguments_list, j, Rf_mkChar(com_function.arguments(j).name().c_str()));
      }

      // function has name, index, type and list of arguments
      SEXP r_function_descriptor = Rf_allocVector(VECSXP, 4);
      SET_VECTOR_ELT(r_function_descriptor, 0, Rf_mkString(com_function.function().name().c_str()));
      SET_VECTOR_ELT(r_function_descriptor, 1, Rf_ScalarInteger(com_function.function().index()));
      SET_VECTOR_ELT(r_function_descriptor, 2, Rf_mkString(call_type.c_str()));
      SET_VECTOR_ELT(r_function_descriptor, 3, r_arguments_list);
      SetNames(r_function_descriptor, {"name", "index", "call.type", "arguments"});

      SET_VECTOR_ELT(functions_list, i, r_function_descriptor);
    }
    SetNames(functions_list, function_names);
    SET_VECTOR_ELT(descriptor, 2, functions_list);
    
    // enums are named, values are lists of (named) ints
    len = com_pointer.enums_size();
    SEXP enums_list = Rf_allocVector(VECSXP, len);
    std::vector< std::string > enum_names;
    for (int i = 0; i < len; i++) {
      auto com_enum = com_pointer.enums(i);
      enum_names.push_back(com_enum.name());
      int enums_len = com_enum.values_size();
      std::vector < std::string > enum_value_names;
      SEXP r_enum_descriptor = Rf_allocVector(INTSXP, enums_len);
      for (int j = 0; j < enums_len; j++) {
        const auto &value_ref = com_enum.values(j);
        INTEGER(r_enum_descriptor)[j] = value_ref.value();
        enum_value_names.push_back(value_ref.name());
      }
      SetNames(r_enum_descriptor, enum_value_names);
      SET_VECTOR_ELT(enums_list, i, r_enum_descriptor); //  Rf_mkString(com_enum.name().c_str()));
    }
    SetNames(enums_list, enum_names);
    SET_VECTOR_ELT(descriptor, 3, enums_list);

    SetNames(descriptor, { "interface", "pointer", "functions", "enums" });

    return descriptor;
  }
  break;

  case BERTBuffers::Variable::ValueCase::kCacheReference:
    return ResolveCacheReference(var.cache_reference());
    break;

  case BERTBuffers::Variable::ValueCase::kErr:
    if (var.err().type() == BERTBuffers::ErrorType::NA) return Rf_ScalarInteger(NA_INTEGER);

  default:
    return R_NilValue;
  }

}

SEXP ResolveCacheReference(uint32_t reference) {

  int err = 0;
  SEXP env = R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString("BERT")), R_GlobalEnv, &err);

  if (!err && Rf_isEnvironment(env)) {
    SEXP object = R_tryEvalSilent(Rf_lang2(Rf_install(".get.cached.object"), Rf_ScalarInteger(reference)), env, &err);
    if (!err) return object;
  }

  return R_NilValue;

}

SEXP RCallSEXP(const BERTBuffers::CompositeFunctionCall &fc, bool wait, int &err) {

  // auto fc = call.function_call();
  err = 0;
  int len = fc.arguments().size();
  int flags = fc.flags();

  if (flags == 1) {

    // this is a mapped function, use special calling syntax...

    SEXP sargs = Rf_allocVector(VECSXP, len+1);
    SET_VECTOR_ELT(sargs, 0, Rf_mkString(fc.function().c_str()));

    for (int i = 0; i < len; i++) {
      SET_VECTOR_ELT(sargs, i + 1, VariableToSEXP(fc.arguments(i)));
    }

    SEXP env = R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString("BERT")), R_GlobalEnv, &err);
    if (!err && Rf_isEnvironment(env)) {
      return R_tryEval(Rf_lang3(Rf_install("do.call"), Rf_mkString(".call.mapped.function"), sargs), env, &err);
    }

    return R_NilValue;

  }
  else {

    int index = 0;
    std::vector<std::string> names;

    for (int i = 0; i < len; i++) {
      const auto &argument = fc.arguments(i);
      if (argument.value_case() != BERTBuffers::Variable::ValueCase::kMissing) {
        if (argument.name() == "...") names.push_back("");
        else names.push_back(argument.name());
      }
    }

    SEXP sargs = Rf_allocVector(VECSXP, names.size());
    PROTECT(sargs);

    for (int i = 0; i < len; i++) {
      const auto &argument = fc.arguments(i);
      if (argument.value_case() != BERTBuffers::Variable::ValueCase::kMissing) {
        SET_VECTOR_ELT(sargs, index++, VariableToSEXP(argument));
      }
    }

    SetNames(sargs, names);
    
    SEXP env = R_GlobalEnv;
    std::vector<std::string> parts;
    StringUtilities::Split(fc.function().c_str(), '$', 0, parts, true);

    // resolve qualified name

    // if you're going to do this, you also need to handle packages,
    // both exported (::) and unexported (:::). I think this is useful,
    // but it's probably expensive.

    // OTOH, if there is a module, it will be the first (and likely only)
    // qualifier.  

    // also note that you can get() the function and pass that to do.call,
    // instead of using the name, which may be easier

    if (parts.size() > 1) {
      for (int i = 0; !err && i < parts.size() - 1; i++) {
        SEXP s = R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString(parts[i].c_str())), env, &err);
        if (!err && Rf_isEnvironment(s)) env = s;
      }
    }

    SEXP call_result = R_tryEval(Rf_lang3(Rf_install("do.call"), Rf_mkString(parts[parts.size() - 1].c_str()), sargs), env, &err);
    UNPROTECT(1);
    return call_result;

  }

}

bool ReadSourceFile(const std::string &file, bool notify) {
  int err = 0;
  if (notify) {
    std::string message = "Loading script file: ";
    message.append(file);
    message.append("\n");
    R_tryEval(Rf_lang2(Rf_install("cat"), Rf_mkString(message.c_str())), R_GlobalEnv, &err);
  }
  R_tryEval(Rf_lang2(Rf_install("source"), Rf_mkString(file.c_str())), R_GlobalEnv, &err);

  return !err;
}

__inline bool HandleSimpleTypes(SEXP sexp, int len, int rtype, BERTBuffers::Array *arr, BERTBuffers::Variable *var) { // , const std::vector<std::string> &levels = {}) {

  if (Rf_isLogical(sexp) || rtype == LGLSXP)
  {
    for (int i = 0; i < len; i++) {
      auto ptr = arr ? arr->add_data() : var;
      int lgl = (INTEGER(sexp))[i];
      if( NA_LOGICAL == lgl ){
        ptr->mutable_err()->set_type(BERTBuffers::ErrorType::NA);
      }
      else {
        ptr->set_boolean(lgl ? true : false);
      }
    }
  }
  else if (Rf_isFactor(sexp)) {

    std::vector<std::string> level_strings;

    SEXP levels = getAttrib(sexp, R_LevelsSymbol);
    if (levels && Rf_isString(levels)) {
      int level_count = Rf_length(levels);
      for (int level = 0; level < level_count; level++) {
        SEXP strsxp = STRING_ELT(levels, level);
        level_strings.push_back(CHAR(Rf_asChar(strsxp)));
      }
    }

    int levels_count = level_strings.size();
    for (int i = 0; i < len; i++) {
      auto ptr = arr ? arr->add_data() : var;
      int level = INTEGER(sexp)[i];
      if (level == NA_INTEGER) {
        ptr->mutable_err()->set_type(BERTBuffers::ErrorType::NA);
      }
      else if (level > 0 && levels_count >= level) ptr->set_str(level_strings[level-1]); // factors are 1-based
      else ptr->set_integer(level);
    }
  }
  else if (Rf_isComplex(sexp)) {

    // handle complex before the catchall rf_isnumber below
    for (int i = 0; i < len; i++) {
      auto ptr = arr ? arr->add_data() : var;
      auto complex = ptr->mutable_cpx();
      if (ISNA(COMPLEX(sexp)[i].i) || ISNA(COMPLEX(sexp)[i].r)) {
        ptr->mutable_err()->set_type(BERTBuffers::ErrorType::NA);
      }
      else {
        complex->set_i(COMPLEX(sexp)[i].i);
        complex->set_r(COMPLEX(sexp)[i].r);
      }
    }

  }
  else if (Rf_isInteger(sexp) || rtype == INTSXP)
  {
    for (int i = 0; i < len; i++) {
      auto ptr = arr ? arr->add_data() : var;
      if (NA_INTEGER == INTEGER(sexp)[i]) {
        ptr->mutable_err()->set_type(BERTBuffers::ErrorType::NA);
      }
      else ptr->set_integer(INTEGER(sexp)[i]);
    }
  }
  else if (isReal(sexp) || Rf_isNumber(sexp) || rtype == REALSXP)
  {
    for (int i = 0; i < len; i++) {
      auto ptr = arr ? arr->add_data() : var;
      if (ISNA(REAL(sexp)[i])) {
        ptr->mutable_err()->set_type(BERTBuffers::ErrorType::NA);
      }
      else ptr->set_real(REAL(sexp)[i]);
    }
  }
  else if (isString(sexp) || rtype == STRSXP)
  {
    for (int i = 0; i < len; i++) {
      auto ptr = arr ? arr->add_data() : var;
      SEXP strsxp = STRING_ELT(sexp, i);
      if (NA_STRING == strsxp){
        ptr->mutable_err()->set_type(BERTBuffers::ErrorType::NA);
      }
      else {
        const char *sexp_string = CHAR(Rf_asChar(strsxp));
        if (!ValidUTF8(sexp_string, 0)) {
          ptr->set_str(WindowsCPToUTF8_2(sexp_string, 0));
        }
        else ptr->set_str(sexp_string);
      }
    }
  }
  else return false;

  return true; // handled
}

void SEXPXlReferenceToVariable(BERTBuffers::Variable *var, SEXP sexp) {

  int type;
  auto sheet_reference = var->mutable_ref();
  SEXP slot;

  slot = R_do_slot(sexp, Rf_mkString("R1"));
  if (slot) {
    type = TYPEOF(slot);
    if (type == INTSXP) sheet_reference->set_start_row(INTEGER(slot)[0]);
    else if (type == REALSXP) sheet_reference->set_start_row(REAL(slot)[0]);
    else std::cerr << "Unexpected type in check excel ref [1]: " << type << std::endl;
  }

  slot = R_do_slot(sexp, Rf_mkString("C1"));
  if (slot) {
    type = TYPEOF(slot);
    if (type == INTSXP) sheet_reference->set_start_column(INTEGER(slot)[0]);
    else if (type == REALSXP) sheet_reference->set_start_column(REAL(slot)[0]);
    else std::cerr << "Unexpected type in check excel ref [2]: " << type << std::endl;
  }

  slot = R_do_slot(sexp, Rf_mkString("R2"));
  if (slot) {
    type = TYPEOF(slot);
    if (type == INTSXP) sheet_reference->set_end_row(INTEGER(slot)[0]);
    else if (type == REALSXP) sheet_reference->set_end_row(REAL(slot)[0]);
    else std::cerr << "Unexpected type in check excel ref [3]: " << type << std::endl;
  }

  slot = R_do_slot(sexp, Rf_mkString("C2"));
  if (slot) {
    type = TYPEOF(slot);
    if (type == INTSXP) sheet_reference->set_end_column(INTEGER(slot)[0]);
    else if (type == REALSXP) sheet_reference->set_end_column(REAL(slot)[0]);
    else std::cerr << "Unexpected type in check excel ref [4]: " << type << std::endl;
  }
  
  slot = R_do_slot(sexp, Rf_mkString("SheetID"));
  if (slot){

    type = TYPEOF(slot);
    if (type == INTSXP && Rf_length(slot) == 2)
    {
      int *p = INTEGER(slot);
      uint64_t ids;
      ids = p[0];
      ids <<= 32;
      ids |= p[1];
      
      sheet_reference->set_sheet_id(ids);
    }
    // ... // else std::cerr << "Unexpected type in check excel ref [5]: " << type << ", " << Rf_length(slot) << std::endl;
  }

  if (sheet_reference->end_row() < sheet_reference->start_row()) sheet_reference->set_end_row(sheet_reference->start_row());
  if (sheet_reference->end_column() < sheet_reference->start_column()) sheet_reference->set_end_column(sheet_reference->start_column());

}

void SEXPToVariable(BERTBuffers::Variable *var, SEXP sexp, std::vector <SEXP> envir_list = std::vector<SEXP>()) {

  if (!sexp || Rf_isNull(sexp)) {
    var->set_nil(true);
    return;
  }

  int len = Rf_length(sexp);
  int rtype = TYPEOF(sexp);

  if (rtype == SYMSXP) {

    // we see this when we have a named value (usually a function argument)
    // that has no value. we want to treat this as missing for COM functions,
    // to leave a space.

    // not sure if this comes up in other contexts, though, in which case
    // we may be causing trouble. on the other hand, prior to this we were
    // not handling it at all; which would result in nil/NULL (the default 
    // variable value case)

    if (len > 1) std::cerr << "WARNING: SYMSXP len > 1" << std::endl;

    // FIXME: variables with default value of F (presumably T as well) are 
    // symbols. we need to handle.

    var->set_missing(true);
    return;
  }

  if (len == 0) {
    var->set_nil(true);
    return;
  }

  if (Rf_isEnvironment(sexp)) {} // ...
  else {

    int nrow = len;
    int ncol = 1;
    BERTBuffers::Array *arr = 0;
        
    if (Rf_isFrame(sexp) && rtype == VECSXP) {

      // this is a list of lists; we want to flatten it and we don't want nested 
      // arrays. for this one, columns is the number of entries in the outer 
      // list, and then we need to look at an inner list to check rows.
  
      ncol = len;
      if (len > 0) {
        SEXP first_list = VECTOR_ELT(sexp, 0);
        nrow = Rf_length(first_list);
      }
      arr = var->mutable_arr();
      arr->set_rows(nrow);
      arr->set_cols(ncol);

      // do we need to change direction?

      int column_count = len; // we're going to reuse that var
      for (int col = 0; col < column_count; col++) {

        SEXP column_list = VECTOR_ELT(sexp, col);
        int column_type = TYPEOF(column_list);
        int column_len = Rf_length(column_list);
        
        HandleSimpleTypes(column_list, column_len, column_type, arr, var);

      }

      {
        // column names
        SEXP names = getAttrib(sexp, R_NamesSymbol);
        int names_len = Rf_length(names);
        if (names && names_len && isString(names)) {
          for (int i = 0; i < names_len; i++) {
            arr->add_colnames()->assign(CHAR(Rf_asChar(STRING_ELT(names, i))));
          }
        }
      }

      {
        // row names
        SEXP names = getAttrib(sexp, R_RowNamesSymbol);
        int names_len = Rf_length(names);
        if (names && names_len && isString(names)) {
          for (int i = 0; i < names_len; i++) {
            arr->add_rownames()->assign(CHAR(Rf_asChar(STRING_ELT(names, i))));
          }
        }
      }

      return;

    }
    
    if (Rf_isMatrix(sexp)) {
      nrow = Rf_nrows(sexp);
      ncol = Rf_ncols(sexp);
    }

    if (len > 1 || rtype == VECSXP) {
      arr = var->mutable_arr();
      arr->set_rows(nrow);
      arr->set_cols(ncol);
    }

    if (HandleSimpleTypes(sexp, len, rtype, arr, var)) {
      // ...
    } 
    else if (rtype == EXTPTRSXP) {

      auto com_pointer = var->mutable_com_pointer();
      com_pointer->set_pointer(reinterpret_cast<uint64_t>(R_ExternalPtrAddr(sexp)));
      std::cout << "read external pointer: " << std::hex << com_pointer->pointer() << std::endl;

    }
    else if (rtype == VECSXP) {
      for (int i = 0; i < len; i++) {
        SEXPToVariable(arr->add_data(), VECTOR_ELT(sexp, i));
      }
    }
    else if (rtype == S4SXP) {

      // there's only one of these we're going to handle -- our reference type
      if (Rf_inherits(sexp, "xlReference")) {
        SEXPXlReferenceToVariable(var, sexp);
      }
      else if (Rf_inherits(sexp, "BERTCacheReference")) {

        uint32_t reference = 0;
        SEXP slot = R_do_slot(sexp, Rf_mkString("reference"));
        if (slot) {
          int type = TYPEOF(slot);
          if (type == INTSXP) reference = INTEGER(slot)[0];
          else if (type == REALSXP) reference = REAL(slot)[0];
        }
        var->set_cache_reference(reference);
      }

    }

    SEXP names = getAttrib(sexp, R_NamesSymbol);
    int names_type = TYPEOF(names);
    if (names && names_type != 0) {
      int nameslen = Rf_length(names);

      // NOTE: issue #117 was caused by returning a LANGSXP type, 
      // which we don't handle because it's not representable. this 
      // parameter had names, but the array was not sized to match
      // (because we are not handling that type).

      int array_len = arr ? arr->data_size() : 1;

      for (int i = 0; i < len && i < nameslen && i < array_len; i++) {
        auto ref = arr ? arr->mutable_data(i) : var;
        SEXP name = STRING_ELT(names, i);
        std::string str(CHAR(Rf_asChar(name)));
        if (str.length()) { ref->set_name(str); }

      }
    }

    SEXP dimnames = getAttrib(sexp, R_DimNamesSymbol);
    if (dimnames && TYPEOF(dimnames) != 0) {
      if (TYPEOF(dimnames) == VECSXP) {
        int dimnames_length = Rf_length(dimnames);
        if (dimnames_length > 0) {
          auto rownames = arr->mutable_rownames();
          SEXP name_list = VECTOR_ELT(dimnames, 0);
          int nameslen = Rf_length(name_list);
          for (int i = 0; i < nameslen; i++)  rownames->Add(CHAR(Rf_asChar(STRING_ELT(name_list, i))));
        }
        if (dimnames_length > 1) {
          auto colnames = arr->mutable_colnames();
          SEXP name_list = VECTOR_ELT(dimnames, 1);
          int nameslen = Rf_length(name_list);
          for (int i = 0; i < nameslen; i++)  colnames->Add(CHAR(Rf_asChar(STRING_ELT(name_list, i))));
        }
        if (dimnames_length > 2) {
          std::cout << " * unhandled dimnames: length is " << dimnames_length << std::endl;
        }
      }
      else {
        std::cout << " * unhandled: dimnames type is " << TYPEOF(dimnames) << std::endl;
      }
    }

  }

}


BERTBuffers::CallResponse& ListScriptFunctions(BERTBuffers::CallResponse &response) {
  
  ParseStatus ps;

  SEXP cmds = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_STRING_ELT(cmds, 0, Rf_mkChar("BERT$list.functions()"));

  SEXP parsed = PROTECT(R_ParseVector(cmds, -1, &ps, R_NilValue));

  if (ps != PARSE_OK) {
    response.set_err("R parse error");
  }
  else {
    SEXP result = R_NilValue;
    int err = 0, len = Rf_length(parsed);
    for (int i = 0; !err && i < Rf_length(parsed); i++) {
      SEXP cmd = VECTOR_ELT(parsed, i);
      result = R_tryEval(cmd, R_GlobalEnv, &err);
    }
    if (err) response.set_err("R error");
    else {

      auto function_list = response.mutable_function_list();

      // we have a kind of complex structure here, which could maybe be simplified 
      // on the R side. it might actually be easier to translate to PBs...

      BERTBuffers::Variable var;
      SEXPToVariable(&var, result);

      // ok so var should now have a list, should be easier to walk

      for (auto function_entry : var.arr().data()) {

        auto descriptor = function_list->add_functions();
        std::vector<std::string> descriptions;

        // we need description sooner rather than later, so let's look for it
        for (auto element : function_entry.arr().data()) {
          if (element.name() == "attributes"){
            if (element.value_case() == BERTBuffers::Variable::ValueCase::kArr) {
              for (auto attribute : element.arr().data()) {
                if (attribute.name() == "description") {
                  if (attribute.value_case() == BERTBuffers::Variable::ValueCase::kArr) {
                    for (auto description : attribute.arr().data()) {
                      descriptions.push_back(description.str());
                    }
                  }
                }
                else if (attribute.name() == "category") {
                  descriptor->set_category(attribute.str());
                }
              }
            }
            break;
          }
        }
        
        for (auto element : function_entry.arr().data()) {
          const std::string &name = element.name();
          if (!name.compare("name")) {
            auto function = descriptor->mutable_function();
            function->set_name(element.str());
            if (descriptions.size() > 0) function->set_description(descriptions[0]);
          }
          else if (!name.compare("flags")) {
            if (element.value_case() == BERTBuffers::Variable::ValueCase::kInteger)
              descriptor->set_flags(element.integer());
            else if (element.value_case() == BERTBuffers::Variable::ValueCase::kReal)
              descriptor->set_flags(element.real());
          }
          else if (!name.compare("arguments")) {
            int description_index = 1;
            for (auto argument : element.arr().data()) {
              auto argument_entry = descriptor->add_arguments();
              if (descriptions.size() > description_index) argument_entry->set_description(descriptions[description_index]);
              for (auto entry : argument.arr().data()) {
                const std::string &entry_name = entry.name();
                if (!entry_name.compare("name")) argument_entry->set_name(entry.str());
                else if (!entry_name.compare("default")) {
                  argument_entry->mutable_default_value()->CopyFrom(entry);
                }
              }
              description_index++;
            }
          }

        }
      }

    }
  }

  UNPROTECT(2);

  return response;
}

BERTBuffers::CallResponse& ExecUserCommand(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call) {

  uint32_t id = call.user_command();
  int err = 0;

  SEXP arguments = Rf_allocVector(VECSXP, 1);
  SET_VECTOR_ELT(arguments, 0, Rf_ScalarInteger(id));

  SEXP env = R_tryEvalSilent(Rf_lang2(Rf_install("get"), Rf_mkString("BERT")), R_GlobalEnv, &err);
  if (!err && Rf_isEnvironment(env)) {
    R_tryEval(Rf_lang3(Rf_install("do.call"), Rf_mkString("ExecUserButton"), arguments), env, &err);
  }

  return response;
}

BERTBuffers::CallResponse& RCall(BERTBuffers::CallResponse &rsp, const BERTBuffers::CallResponse &call) {

  int err = 0;
  bool wait = call.wait();

  SEXP result = PROTECT(RCallSEXP(call.function_call(), wait, err));

  if (err) {
    rsp.set_err("parse error");
  }
  else
  {
    // we don't need to convert the result if we are not going to send it
    if (wait) SEXPToVariable(rsp.mutable_result(), result);
  }
  UNPROTECT(1);

  return rsp;
}

BERTBuffers::CallResponse& RExec(BERTBuffers::CallResponse &rsp, const BERTBuffers::CallResponse &call) {

  auto code = call.code();
  int count = code.line_size();

  ParseStatus ps;

  if (count == 0) {
    auto val = rsp.mutable_result();
    val->set_integer(0);
    return rsp;
  }

  SEXP cmds = PROTECT(Rf_allocVector(STRSXP, count));

  for (int i = 0; i < count; i++) {
    // temp
    const std::string &line = code.line(i);
    SET_STRING_ELT(cmds, i, Rf_mkChar(code.line(i).c_str()));
  }

  SEXP parsed = PROTECT(R_ParseVector(cmds, -1, &ps, R_NilValue));

  if (ps != PARSE_OK) {
    rsp.set_err("R parse error");
  }
  else {
    SEXP result = R_NilValue;
    int err = 0, len = Rf_length(parsed);
    for (int i = 0; !err && i < Rf_length(parsed); i++) {
      SEXP cmd = VECTOR_ELT(parsed, i);
      result = R_tryEval(cmd, R_GlobalEnv, &err);
    }
    if (err) rsp.set_err("R error");
    else if (call.wait()) SEXPToVariable(rsp.mutable_result(), result);
  }

  UNPROTECT(2);

  return rsp;
}

SEXP COMCallback(SEXP function_name, SEXP call_type, SEXP index, SEXP pointer_key, SEXP arguments) {

  static uint32_t callback_id = 1;
  SEXP sexp_result = R_NilValue;

  std::string string_name;
  std::string string_type;

  if (isString(function_name)) string_name = (CHAR(Rf_asChar(STRING_ELT(function_name, 0))));
  if (isString(call_type)) string_type = (CHAR(Rf_asChar(STRING_ELT(call_type, 0))));

  if (!string_name.length()) return R_NilValue;

  BERTBuffers::CallResponse *call = new BERTBuffers::CallResponse;
  BERTBuffers::CallResponse *response = new BERTBuffers::CallResponse;

  call->set_id(callback_id);
  call->set_wait(true);

  //auto callback = call->mutable_com_callback();
  auto callback = call->mutable_function_call();
  callback->set_target(BERTBuffers::CallTarget::COM);
  callback->set_function(string_name);

  // index may be passed as a double instead of an int, convert
  uint32_t call_index = 0;

  if (Rf_isInteger(index)) call_index = (INTEGER(index))[0];
  else if (Rf_isReal(index)) call_index = (uint32_t)((REAL(index))[0]);
  callback->set_index(call_index);

  callback->set_pointer(reinterpret_cast<uint64_t>(R_ExternalPtrAddr(pointer_key)));

  if (!string_type.compare("get")) callback->set_type(BERTBuffers::CallType::get);
  else if (!string_type.compare("put")) callback->set_type(BERTBuffers::CallType::put);
  else if (!string_type.compare("method")) callback->set_type(BERTBuffers::CallType::method);

  // we can unpack arguments here, no need to pass an array

  if (arguments) {
    if (TYPEOF(arguments) == VECSXP) {
      int arguments_len = Rf_length(arguments);
      for (int i = 0; i < arguments_len; i++) {
        SEXPToVariable(callback->add_arguments(), VECTOR_ELT(arguments, i));
      }
    }

    // should not happen, so we should not do this -- we're not 
    // expecting it on the other end

    else SEXPToVariable(callback->add_arguments(), arguments);
  }

  bool success = Callback(*call, *response);

  if (success) {
    int err = 0;
    if (response->operation_case() == BERTBuffers::CallResponse::OperationCase::kFunctionCall) {
      sexp_result = RCallSEXP(response->function_call(), true, err);
    }
    else if (response->operation_case() == BERTBuffers::CallResponse::OperationCase::kResult
      && response->result().value_case() == BERTBuffers::Variable::ValueCase::kComPointer) {

      BERTBuffers::CompositeFunctionCall *function_call = new BERTBuffers::CompositeFunctionCall;
      function_call->set_function("BERT$install.com.pointer");
      auto argument = function_call->add_arguments();

      // I want to borrow this, not copy, can we do that?
      argument->mutable_com_pointer()->CopyFrom(response->result().com_pointer());

      sexp_result = RCallSEXP(*function_call, true, err);
      //argument->release_com_pointer();
      delete function_call;

    }
    else sexp_result = VariableToSEXP(response->result());
  }

  delete call;
  delete response;

  if (!success) {
    error_return("internal method failed");
  }

  return sexp_result;
}

void UpdateSpreadsheetGraphics() {

  uint32_t graphics_update_id = 0;

  std::vector<gdi_graphics_device::GraphicsUpdateRecord> update_list = SpreadsheetGraphicsDevice::UpdatePendingGraphics();

  if (update_list.size()) {

    BERTBuffers::CallResponse message, response;
    message.set_id(graphics_update_id++);
    message.set_wait(true);

    auto function_call = message.mutable_function_call();
    function_call->set_target(BERTBuffers::CallTarget::graphics);
    for (const auto &device : update_list) {

      auto argument = function_call->add_arguments();
      auto graphics = argument->mutable_graphics();

      graphics->set_width(device.width);
      graphics->set_height(device.height);
      graphics->set_name(device.name);
      graphics->set_path(device.path);

    }

    Callback(message, response);

  }
}

SEXP RCallback(SEXP command, SEXP data) {

  static uint32_t callback_id = 1;
  SEXP sexp_result = R_NilValue;
  std::string string_command;

  if (isString(command)) {
    string_command = (CHAR(Rf_asChar(STRING_ELT(command, 0))));
  }

  if (!string_command.length()) {
    return R_NilValue;
  }

  // some commands are handled here. others are sent to BERT as callbacks.

  if (!string_command.compare("console-device")) {

    // validate args?
    if (TYPEOF(data) != VECSXP) return Rf_ScalarLogical(0);
    if (Rf_length(data) != 6) return Rf_ScalarLogical(0);

    std::string background, type;
    double width, height, pointsize;
    void *pointer;

    background = CHAR(Rf_asChar(VECTOR_ELT(data, 0)));
    width = Rf_asReal(VECTOR_ELT(data, 1));
    height = Rf_asReal(VECTOR_ELT(data, 2));
    pointsize = Rf_asReal(VECTOR_ELT(data, 3));
    type = CHAR(Rf_asChar(VECTOR_ELT(data, 4)));
    pointer = R_ExternalPtrAddr(VECTOR_ELT(data, 5));

    return ConsoleGraphicsDevice::CreateConsoleDevice(background, width, height, pointsize, type, pointer);
  }
  if (!string_command.compare("spreadsheet-device")) {

    // validate args?
    if (TYPEOF(data) != VECSXP) return Rf_ScalarLogical(0);
    if (Rf_length(data) != 6) return Rf_ScalarLogical(0);

    std::string background, name;
    double width, height, pointsize;
    void *pointer;

    name = CHAR(Rf_asChar(VECTOR_ELT(data, 0)));
    background = CHAR(Rf_asChar(VECTOR_ELT(data, 1)));
    width = Rf_asReal(VECTOR_ELT(data, 2));
    height = Rf_asReal(VECTOR_ELT(data, 3));
    pointsize = Rf_asReal(VECTOR_ELT(data, 4));
    pointer = R_ExternalPtrAddr(VECTOR_ELT(data, 5));

    return SpreadsheetGraphicsDevice::CreateSpreadsheetDevice(name, background, width, height, pointsize, pointer);
  }
  else if (!string_command.compare("spreadsheet-device")) {
    std::cerr << "ENOTIMPL: " << string_command << std::endl;
    return Rf_ScalarLogical(0);
  }
  else if (!string_command.compare("console-history")) {
    
    BERTBuffers::CallResponse message, response;
    auto history = message.mutable_console()->mutable_history();
    SEXPToVariable(history, data);
    bool success = ConsoleCallback(message, response);
    if (success) {
      if (response.operation_case() == BERTBuffers::CallResponse::OperationCase::kResult) {
        return VariableToSEXP(response.result());
      }
    }
    return R_NilValue;
  }
  
  BERTBuffers::CallResponse *call = new BERTBuffers::CallResponse;
  BERTBuffers::CallResponse *response = new BERTBuffers::CallResponse;

  call->set_id(callback_id++);
  call->set_wait(true);

  if (!string_command.compare("remap-functions")) {
    ListScriptFunctions(*call);
  }
  else {
    auto callback = call->mutable_function_call();
    callback->set_function(string_command);
    if (data) SEXPToVariable(callback->add_arguments(), reinterpret_cast<SEXP>(data));
  }

  bool success = Callback(*call, *response);
  if (success) sexp_result = VariableToSEXP(response->result());

  delete call;
  delete response;

  if (!success) {
    error_return("internal method failed");
  }

  return sexp_result;
}
