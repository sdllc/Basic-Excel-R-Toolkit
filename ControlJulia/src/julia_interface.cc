
#include <iostream>
#include "../../PB/variable.pb.h"

#include "julia_interface.h"

#include <fstream>

//
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <inttypes.h>

#include "string_utilities.h"

#define JULIA_ENABLE_THREADING 1

#include "julia.h"

jl_ptls_t ptls;

jl_value_t * VariableToJlValue(const BERTBuffers::Variable *variable) {

  jl_value_t* value = jl_nothing;

  switch (variable->value_case()) {
  case BERTBuffers::Variable::kBoolean:
    value = jl_box_bool(variable->boolean());
    break;

  case BERTBuffers::Variable::kNum:
    value = jl_box_float64(variable->num());
    break;

  case BERTBuffers::Variable::kStr:
    value = jl_pchar_to_string(variable->str().c_str(), variable->str().length());
    break;

  case BERTBuffers::Variable::kExternalPointer:

    // FIXME: we should construct a wrapper type for this. the below is sufficient
    // for preserving value (unlike R, which can't accept the value), but might lead
    // it to being treated differently. also we need to register a finalizer...

    value = jl_box_uint64(variable->external_pointer());
    break;

  case BERTBuffers::Variable::kArr:
  {
    const BERTBuffers::Array &arr = variable->arr();

    int nrows = arr.rows();
    int ncols = arr.cols();
    int len = arr.data_size();

    if (!nrows || !ncols || len != (nrows * ncols)) {
      ncols = 1;
      nrows = len;
    }

    // FIXME: names?

    // FIXME: types? we should either determine (here or at array creation time)
    // if the array is a single type (we already do this somewhat for R); then we
    // can use more efficient julia arrays and use a data pointer rather than the 
    // set() syntax

    jl_array_t *julia_array = 0; //  jl_nothing;
    if (ncols == 1) 
    {
      jl_value_t* array_type = jl_apply_array_type((jl_value_t*)jl_any_type, 1);
      julia_array = jl_alloc_array_1d(array_type, nrows);

      for (size_t i = 0; i < nrows; i++) {
        jl_value_t *element = VariableToJlValue(&(arr.data(i)));
        jl_arrayset(julia_array, element, i);
      }

    }
    else {

      jl_value_t* array_type = jl_apply_array_type((jl_value_t*)jl_any_type, 2);
      julia_array = jl_alloc_array_2d(array_type, nrows, ncols);

      for (size_t i = 0; i < ncols; i++) {
        for (size_t j = 0; j < nrows; j++) {
          jl_value_t *element = VariableToJlValue(&(arr.data(i)));
          jl_arrayset(julia_array, element, j + nrows * i);
        }
      }

    }
    
    value = (jl_value_t*)julia_array;
    break;
  }

  default:
    std::cout << "Unhandled type in variable to jlvalue: " << variable->value_case() << std::endl;
    break;
  }

  if (variable->name().length()) {

    // create tuple with name, value. the below creates a "DataType" type.
    // I'd prefer this to be a typed tuple, but one thing at a time.

    jl_value_t* jl_name = jl_pchar_to_string(variable->name().c_str(), variable->name().length());

    jl_svec_t * svec = jl_svec2(jl_name, value);

//    jl_value_t* tuple[] = { jl_name, value };
//    return (jl_value_t*)jl_apply_tuple_type_v(tuple, 2);
//    return jl_new_structv(jl_any_type, tuple, 2);

    return (jl_value_t*) jl_apply_tuple_type(svec);


  }

  return value;
  
}

void JlValueToVariable(BERTBuffers::Variable *variable, jl_value_t *value) {

  // nothing/null/nil
  if (jl_is_nothing(value)) {
    variable->set_nil(true);
    return;
  }
  
  // string [...]
  if (jl_typeis(value, jl_string_type)) {
    variable->set_str(std::string(jl_string_ptr(value), jl_string_len(value)));
    return;
  }

  // boolean
  if (jl_typeis(value, jl_bool_type)) {
    variable->set_boolean(jl_unbox_bool(value));
    return;
  }

  // floats
  if (jl_typeis(value, jl_float64_type)) {
    variable->set_num(jl_unbox_float64(value));
    return;
  }
  if (jl_typeis(value, jl_float32_type)) {
    variable->set_num(jl_unbox_float32(value));
    return;
  }

  // ints
  if (jl_typeis(value, jl_int64_type)) {
    variable->set_num(jl_unbox_int64(value));
    return;
  }
  if (jl_typeis(value, jl_int32_type)) {
    variable->set_num(jl_unbox_int32(value));
    return;
  }
  if (jl_typeis(value, jl_int16_type)) {
    variable->set_num(jl_unbox_int16(value));
    return;
  }
  if (jl_typeis(value, jl_int8_type)) {
    variable->set_num(jl_unbox_int8(value));
    return;
  }

  // complex

  // tuple!

  // array

  if (jl_is_array(value)) {

    auto results_array = variable->mutable_arr();
    auto jl_array = (jl_array_t*)value;
    auto eltype = jl_array_eltype(value);

    int ncols = jl_array->ncols;
    int nrows = jl_array->nrows;
    int len = jl_array->length;

    // std::cout << "arr: " << ncols << ", " << nrows << ", " << len << "; p? " << (jl_array->flags.ptrarray != 0) << std::endl;

    if (jl_array->flags.ptrarray) {
      jl_value_t** data = (jl_value_t**)(jl_array_data(jl_array));
      for (int i = 0; i < len; i++) JlValueToVariable(results_array->add_data(), data[i]);
      return;
    }
    else if (eltype == jl_float64_type) {
      double *d = (double*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_num(d[i]);
      return;
    }
    else if (eltype == jl_float32_type) {
      float *d = (float*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_num(d[i]);
      return;
    }
    else if (eltype == jl_int64_type ) {
      int64_t *d = (int64_t*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_num(d[i]);
      return;
    }
    else if (eltype == jl_int32_type) {
      int32_t *d = (int32_t*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_num(d[i]);
      return;
    }
    else if (eltype == jl_int8_type) {
      int8_t *d = (int8_t*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_num(d[i]);
      return;
    }
    else if (eltype == jl_bool_type) {
      int8_t *d = (int8_t*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_boolean(d[i]);
      return;
    }
   
    // string? 

    // pointer?



  }

  jl_value_t *value_type = jl_typeof(value);
  jl_printf(JL_STDOUT, "unexpected type: ");
  jl_static_show(JL_STDOUT, value_type);
  jl_printf(JL_STDOUT, "\n");


  // ?

  BERTBuffers::Error *err = variable->mutable_err();
  err->set_message("Unexpected variable type");

}

void JuliaInit() {

  //memset(&jl_options, 0, sizeof(jl_options));
  //jl_options.quiet = 0;
  //jl_options.banner = 1;
  jl_options.quiet = 0;
  jl_options.color = JL_OPTIONS_COLOR_ON;

  ptls = jl_get_ptls_states();

  /* required: setup the Julia context */
  jl_init();

  // FIXME (NEXT): need to redirect stdout/stderr to streams we can
  // push out on the pipes (or cache)

  /* ... */
  // jl_eval_string("Base.banner()");

}

void JuliaShutdown() {

  /* strongly recommended: notify Julia that the
  program is about to terminate. this allows
  Julia time to cleanup pending write requests
  and run all finalizers
  */
  jl_atexit_hook(0);

}

void ReportJuliaException(const char *tag, bool backtrace = false) {

  std::cout << " * CATCH [" << tag << "]" << std::endl;

  if (backtrace) jlbacktrace();

  jl_value_t *jl_stderr = jl_get_global(jl_main_module, jl_symbol("STDERR"));
  if (jl_stderr == jl_nothing) {
    jl_static_show(JL_STDERR, ptls->exception_in_transit);
  }
  else {
    jl_call2(jl_get_function(jl_main_module, "showerror"), jl_stderr, ptls->exception_in_transit);
  }

  jl_printf(JL_STDERR, "\n\n"); // matches julia repl

  jl_exception_clear();

}

bool ReadSourceFile(const std::string &file) {

  jl_function_t *function_pointer = jl_get_function(jl_main_module, "include");
  if (!function_pointer || jl_is_nothing(function_pointer)) return false;

  jl_value_t *function_result;
  jl_value_t *argument = jl_pchar_to_string(file.c_str(), file.length());

  JL_TRY{
    function_result = jl_call1(function_pointer, argument);
    if (jl_exception_occurred()) {
      std::cout << "* [RSF] EXCEPTION" << std::endl;
      jl_printf(JL_STDERR, "[RSF] error during run:\n");
      jl_static_show(JL_STDERR, ptls->exception_in_transit);
      jl_exception_clear();
    }
    else return true; // success
  }
  JL_CATCH{
    std::cout << "* CATCH [RSF]" << std::endl;
    jl_printf(JL_STDERR, "\nparser error:\n");
    jl_static_show(JL_STDERR, ptls->exception_in_transit);
    jl_printf(JL_STDERR, "\n");
    jlbacktrace();
    jl_exception_clear();
  }

  return false;
}


/**
 * shell exec: response is printed to output stream. 
 * 
 * shell supports multi-line entry, which may be incomplete as of
 * any given line. previous lines are stored in a buffer (now as a 
 * concatenated string). Ctrl+C or other break should clear buffer.
 *
 * this function is not responsible for maintaining or appending
 * the buffer. caller can do that (if desired) when response is 
 * "incomplete".
 */
ExecResult JuliaShellExec(const std::string &command, const std::string &shell_buffer) {

  static char filename[] = "shell";
  static int filename_len = strlen(filename);

  std::string tmp = shell_buffer;
  tmp += command;
  
  // this is a little hacky, but we are trying to emulate the 
  // stock julia parser to the extent that's reasonable

  if (StringUtilities::EndsWith(StringUtilities::Trim(tmp), ";")) {
    tmp = tmp + "nothing";
  }
  
  // same: help commands start with ?

  // FIXME: clean up strings that will break this?
  // FIXME: handle this in shell, don't pass through

  if (tmp.length() && tmp.c_str()[0] == '?') {
    std::string help = "eval(Base.Docs.helpmode(\"";
    help += tmp.substr(1);
    help += "\"))";
    tmp = help;
  }

  ExecResult result = ExecResult::Success;

  JL_TRY{

    jl_value_t *val = jl_parse_input_line(tmp.c_str(), tmp.length(), filename, filename_len);

    // when does this get set, vs. throwing?

    if (jl_exception_occurred()) {
      std::cout << "* [JSE] EXCEPTION" << std::endl;
      std::cout << "command was: " << tmp << std::endl;

      jl_printf(JL_STDERR, "[JSE] error during run:\n");
      jl_static_show(JL_STDERR, ptls->exception_in_transit);
      jl_exception_clear();
      result = ExecResult::Error;
    }
    else if (val) {

      if (jl_is_expr(val) && (((jl_expr_t*)val)->head == jl_incomplete_sym)){
        result = ExecResult::Incomplete;
      }
      else if(result == ExecResult::Success) {

        // from jl_eval_string

        JL_GC_PUSH1(&val);
        size_t last_age = jl_get_ptls_states()->world_age;
        jl_get_ptls_states()->world_age = jl_get_world_counter();
        jl_value_t *r = jl_toplevel_eval_in(jl_main_module, val);
        jl_get_ptls_states()->world_age = last_age;
        JL_GC_POP();
        jl_exception_clear();

        if (!jl_is_nothing(r)) {

          // better for some, not for others. we need to figure out
          // what repl is doing with output.

          //jl_static_show(JL_STDOUT, r);
          jl_call1(jl_get_function(jl_main_module, "print"), r);

          jl_printf(JL_STDOUT, "\n");
        }

        // to match julia REPL, always an extra newline
        jl_printf(JL_STDOUT, "\n");

      }
    }

  }
  JL_CATCH{
    ReportJuliaException("JSE");
    result = ExecResult::Error;
  }

  while( true ){
    int remaining_handles = uv_run(jl_global_event_loop(), UV_RUN_NOWAIT);
    if (!remaining_handles) break;
  }

  return result;

}

void JuliaCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call) {

  response.set_id(call.id());

  std::string function = call.function_call().function();

  // lookup in main includes our defined functions plus (apparently) Base, which 
  // is attached is some fashion. can we dereference pacakges? [A: no, probably 
  // need to do that manually].

  jl_function_t *function_pointer = jl_nothing;

  std::vector<std::string> elements;
  StringUtilities::Split(function, '.', 1, elements);

  if( elements.size() == 1 ) function_pointer = jl_get_function(jl_main_module, function.c_str());
  else {
    function_pointer = jl_get_global(jl_main_module, jl_symbol(elements[0].c_str()));
    for( int i = 1; i< elements.size(); i++ ) function_pointer = jl_get_global((jl_module_t*)function_pointer, jl_symbol(elements[i].c_str()));
  }

  if (!function_pointer || jl_is_nothing(function_pointer)) return;
  jl_value_t *function_result;

  JL_TRY {

    // call here, with or without arguments
    int len = call.function_call().arguments().size();
    if (len > 0) {
      std::vector<jl_value_t*> arguments_vector;
      for (auto argument : call.function_call().arguments()) {
        arguments_vector.push_back(VariableToJlValue(&argument));
      }
      function_result = jl_call(function_pointer, &(arguments_vector[0]), len);
    }
    else {
      function_result = jl_call0(function_pointer);
    }

    // check for a julia exception (handled)
    if (jl_exception_occurred()) {
      std::cout << "* [JC] EXCEPTION" << std::endl;
      jl_printf(JL_STDERR, "[JC] error during run:\n");
      jl_static_show(JL_STDERR, ptls->exception_in_transit);
      jl_exception_clear();
      jl_printf(JL_STDOUT, "\n");

      // set err
      response.set_err("julia exception");
    }
    else 
    {
      // success: return result or nil as an empty success value
      if (function_result) {
        JlValueToVariable(response.mutable_result(), function_result);
      }
      else {
        response.mutable_result()->set_nil(true);
      }
    }

  }
  JL_CATCH {

    // external exception; return error

    std::cout << "* CATCH [JC]" << std::endl;
    jl_printf(JL_STDERR, "\nparser error:\n");

    jl_static_show(JL_STDERR, ptls->exception_in_transit);
    jl_printf(JL_STDERR, "\n");
    jlbacktrace();

    jl_exception_clear();

    response.set_err("external exception");

  }

  // JlValueToVariable(response.mutable_result(), function_result);

}

void JuliaExec(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call) {

  response.set_id(call.id());

  std::string composite;
  for (auto line : call.code().line()) {
    // std::cout << "[line] " << line << std::endl;
    composite += line;
    composite += "\n";
  }

  JL_TRY{

    jl_value_t *val = (jl_value_t*)jl_load_file_string(composite.c_str(), composite.length(), "inline");

    // ok so this gets called if there is an exception but we caught it; 
    // why does the catch not remove it entirely? (...)

    if (jl_exception_occurred()) {
      std::cout << "* [JE] EXCEPTION" << std::endl;
      //jl_printf(JL_STDERR, "[JE] error during run:\n");
      //jl_static_show(JL_STDERR, ptls->exception_in_transit);
      jl_exception_clear();
    }
    
    if (val) {
      JlValueToVariable(response.mutable_result(), val);
      //jl_static_show(JL_STDOUT, val);
    }
    //jl_printf(JL_STDOUT, "\n");
  }
  JL_CATCH {
    std::cout << "* CATCH [JE]" << std::endl;
    jl_printf(JL_STDERR, "\nparser error:\n");
    jl_static_show(JL_STDERR, ptls->exception_in_transit);
    jl_printf(JL_STDERR, "\n");
    jlbacktrace();
    jl_exception_clear();
  }


}

