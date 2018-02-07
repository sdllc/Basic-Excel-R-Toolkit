/*
 * controlR
 * Copyright (C) 2016 Structured Data, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "variable.pb.h"

#include "controlr_rinterface.h"
#include "controlr_common.h"
#include "util.hpp"

extern unsigned long long GetTimeMs64();

// std::vector< std::string > cmdBuffer;

// try to store fuel now, you jerks
#undef clear
#undef length

/*
#include <google\protobuf\util\json_util.h>

/ ** debug/util function * /
void DumpJSON(const google::protobuf::Message &message, const char *path = 0) {
  std::string str;
  google::protobuf::util::JsonOptions opts;
  opts.add_whitespace = true;
  google::protobuf::util::MessageToJsonString(message, &str, opts);
  if (path) {
    FILE *f;
    fopen_s(&f, path, "w");
    if (f) {
      fwrite(str.c_str(), sizeof(char), str.length(), f);
      fflush(f);
    }
    fclose(f);
  }
  else std::cout << str << std::endl;
}
*/

extern bool Callback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

// forward
void SEXPToVariable(BERTBuffers::Variable *var, SEXP sexp, std::vector < SEXP > envir_list = std::vector < SEXP >());

// forward
void ReleaseExternalPointer(SEXP external_pointer);

extern "C" {
  extern void Rf_PrintWarnings();
  extern Rboolean R_Visible;
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

//  case BERTBuffers::Variable::ValueCase::kNum:
//    return Rf_ScalarReal(var.num());

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

    bool is_numeric = true;
    bool is_integer = true;

    for (int i = 0; i < count && is_numeric; i++) {
      BERTBuffers::Variable::ValueCase value_case = arr.data(i).value_case();
      is_integer = is_integer && (value_case == BERTBuffers::Variable::ValueCase::kInteger);
      is_numeric = is_numeric && (value_case == BERTBuffers::Variable::ValueCase::kReal || value_case == BERTBuffers::Variable::ValueCase::kInteger);
    }

    bool has_names = false;

    for (int i = 0; i < count && !has_names; i++) {
      has_names = has_names || arr.data(i).name().length();
    }

    // FIXME: like numeric, use typed list for single-type fields (string, int, bool, ...?)

    SEXP list;
    if (is_numeric) {
      if (is_integer) {
        if (!cols) list = Rf_allocVector(INTSXP, count);
        else list = Rf_allocMatrix(INTSXP, rows, cols);
        int *p = INTEGER(list);
        for (int i = 0; i < count; i++) p[i] = arr.data(i).integer();
      }
      else {
        if (!cols) list = Rf_allocVector(REALSXP, count);
        else list = Rf_allocMatrix(REALSXP, rows, cols);
        double *p = REAL(list);
        for (int i = 0; i < count; i++) {
          if( arr.data(i).value_case() == BERTBuffers::Variable::ValueCase::kInteger) p[i] = arr.data(i).integer();
          else p[i] = arr.data(i).real();
        }
      }
    }
    else {
      if (!cols) list = Rf_allocVector(VECSXP, count);
      else list = Rf_allocMatrix(VECSXP, rows, cols);
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

  case BERTBuffers::Variable::ValueCase::kErr:
    if (var.err().type() == BERTBuffers::ErrorType::NA) return Rf_ScalarInteger(NA_INTEGER);

  default:
    return R_NilValue;
  }

}

SEXP RCallSEXP(const BERTBuffers::CompositeFunctionCall &fc, bool wait, int &err) {

  // auto fc = call.function_call();
  err = 0;
  int len = fc.arguments().size();

  SEXP sargs = Rf_allocVector(VECSXP, len);
  for (int i = 0; i < len; i++) {
    SET_VECTOR_ELT(sargs, i, VariableToSEXP(fc.arguments(i)));
  }

  SEXP env = R_GlobalEnv;
  std::vector<std::string> parts;
  Util::split(fc.function().c_str(), '$', 0, parts, true);

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

  return R_tryEval(Rf_lang3(Rf_install("do.call"), Rf_mkString(parts[parts.size() - 1].c_str()), sargs), env, &err);

}

bool ReadSourceFile(const std::string &file) {
  int err = 0;
  R_tryEval(Rf_lang2(Rf_install("source"), Rf_mkString(file.c_str())), R_GlobalEnv, &err);
  return !err;
}

BERTBuffers::CallResponse& ListScriptFunctions(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call) {

  response.set_id(call.id());

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

        for (auto element : function_entry.arr().data()) {
          const std::string &name = element.name();
          if (!name.compare("name")) {
            descriptor->mutable_function()->set_name(element.str());
          }
          else if (!name.compare("arguments")) {
            for (auto argument : element.arr().data()) {
              auto argument_entry = descriptor->add_arguments();
              for (auto entry : argument.arr().data()) {
                const std::string &entry_name = entry.name();
                if (!entry_name.compare("name")) argument_entry->set_name(entry.str());
                else if (!entry_name.compare("default")) {
                  argument_entry->mutable_default_value()->CopyFrom(entry);
                }
              }
            }
          }
          else if (!name.compare("attributes")) {
            // ...
          }

        }
      }

    }
  }

  UNPROTECT(2);

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

void SEXPToVariable(BERTBuffers::Variable *var, SEXP sexp, std::vector < SEXP > envir_list) {

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

    if (Rf_isMatrix(sexp)) {
      nrow = Rf_nrows(sexp);
      ncol = Rf_ncols(sexp);
    }

    BERTBuffers::Array *arr = 0;
    if (len > 1 || rtype == VECSXP) {
      arr = var->mutable_arr();
      arr->set_rows(nrow);
      arr->set_cols(ncol);
    }

    // FIXME: should be able to template some of this 

    if (Rf_isLogical(sexp))
    {
      for (int i = 0; i < len; i++) {
        auto ptr = arr ? arr->add_data() : var;
        int lgl = (INTEGER(sexp))[i];
        if (lgl == NA_LOGICAL) {
          ptr->mutable_err()->set_type(BERTBuffers::ErrorType::NA);
        }
        else {
          ptr->set_boolean(lgl ? true : false);
        }
      }
    }
    else if (Rf_isInteger(sexp))
    {
      for (int i = 0; i < len; i++) {
        auto ptr = arr ? arr->add_data() : var;
        ptr->set_integer(INTEGER(sexp)[i]);
      }
    }
    else if (isReal(sexp) || Rf_isNumber(sexp))
    {
      for (int i = 0; i < len; i++) {
        auto ptr = arr ? arr->add_data() : var;
        ptr->set_real(REAL(sexp)[i]);
      }
    }
    else if (isString(sexp))
    {
      for (int i = 0; i < len; i++) {
        auto ptr = arr ? arr->add_data() : var;
        SEXP strsxp = STRING_ELT(sexp, i);
        ptr->set_str(CHAR(Rf_asChar(strsxp)));
      }
    }
    else if (rtype == EXTPTRSXP) {
      //var->set_external_pointer(reinterpret_cast<uint64_t>(R_ExternalPtrAddr(sexp)));
      //std::cout << "read external pointer: " << std::hex << var->external_pointer() << std::endl;

      auto com_pointer = var->mutable_com_pointer();
      com_pointer->set_pointer(reinterpret_cast<uint64_t>(R_ExternalPtrAddr(sexp)));
      std::cout << "read external pointer: " << std::hex << com_pointer->pointer() << std::endl;

    }
    else if (rtype == VECSXP) {
      for (int i = 0; i < len; i++) {
        SEXPToVariable(arr->add_data(), VECTOR_ELT(sexp, i));
      }
    }

    /*
    else if (rtype == SYMSXP) {

        if (symbol_as_missing) {

            // we see symbol types for empty arguments in constructed
            // COM functions. however I'm not sure that's the _only_
            // time we see symbols, so this is problematic.

            // on the other hand, we were not handling this before at
            // all, so it's not going to be any more broken.

            for (int i = 0; i < len; i++) {
                arr->add_data()->set_missing(true);
            }

        }
    }
    */

    SEXP names = getAttrib(sexp, R_NamesSymbol);
    if (names && TYPEOF(names) != 0) {
      int nameslen = Rf_length(names);
      for (int i = 0; i < len && i < nameslen; i++) {
        auto ref = arr ? arr->mutable_data(i) : var;
        SEXP name = STRING_ELT(names, i);
        std::string str(CHAR(Rf_asChar(name)));
        if (str.length()) { ref->set_name(str); }
      }

    }

  }

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

  //int key = (int)((intptr_t)R_ExternalPtrAddr(pointer_key));
  //if (key) callback->set_pointer(key);
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

SEXP RCallback(SEXP command, SEXP data) {

  static uint32_t callback_id = 1;
  SEXP sexp_result = R_NilValue;
  std::string string_command;

  if (isString(command)) {
    string_command = (CHAR(Rf_asChar(STRING_ELT(command, 0))));
  }

  // FIXME: support numeric commands, convert?

  // ...

  if (!string_command.length()) {
    return R_NilValue;
  }

  BERTBuffers::CallResponse *call = new BERTBuffers::CallResponse;
  BERTBuffers::CallResponse *response = new BERTBuffers::CallResponse;

  call->set_id(callback_id);
  call->set_wait(true);
  auto callback = call->mutable_function_call();
  callback->set_function(string_command);

  if (data) SEXPToVariable(callback->add_arguments(), reinterpret_cast<SEXP>(data));

  bool success = Callback(*call, *response);

  // cout << "callback (2) complete (" << success << ")" << endl;

  if (success) sexp_result = VariableToSEXP(response->result());

  delete call;
  delete response;

  if (!success) {
    error_return("internal method failed");
  }

  return sexp_result;
}

void ReleaseExternalPointer(SEXP external_pointer) {
  //int key = (int)((intptr_t)R_ExternalPtrAddr(pointer));

  RCallback(Rf_mkString("release-pointer"), external_pointer);

}

SEXP ExternalCallback(int command_id, void* a, void* b) {

  static uint32_t callback_id = 1;
  SEXP sexp_result = R_NilValue;

  // it looks like we pass SEXPs as these arguments. not sure for 
  // the rationale having them as void*, although it should be fine

  std::stringstream ss;
  ss << command_id;

  BERTBuffers::CallResponse *call = new BERTBuffers::CallResponse;
  BERTBuffers::CallResponse *response = new BERTBuffers::CallResponse;

  call->set_id(callback_id);
  call->set_wait(true);
  auto callback = call->mutable_function_call();
  callback->set_function(ss.str());

  // allow two arguments but not null, argument
  if (a) {
    SEXPToVariable(callback->add_arguments(), reinterpret_cast<SEXP>(a));
    if (b) SEXPToVariable(callback->add_arguments(), reinterpret_cast<SEXP>(b));
  }

  bool success = Callback(*call, *response);

  // cout << "callback complete (" << success << ")" << endl;

  if (success) sexp_result = VariableToSEXP(response->result());


  delete call;
  delete response;

  if (!success) {
    error_return("internal method failed");
  }

  return sexp_result;
}

