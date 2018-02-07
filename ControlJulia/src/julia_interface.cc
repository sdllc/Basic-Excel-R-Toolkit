
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

jl_value_t *JuliaCallJlValue(const BERTBuffers::CompositeFunctionCall &call);

jl_ptls_t ptls; 

jl_value_t * VariableToJlValue(const BERTBuffers::Variable *variable) {

  jl_value_t* value = jl_nothing;

  switch (variable->value_case()) {
  case BERTBuffers::Variable::kBoolean:
    value = jl_box_bool(variable->boolean());
    break;

  case BERTBuffers::Variable::kReal:
    value = jl_box_float64(variable->real());
    break;

  case BERTBuffers::Variable::kInteger:
    value = jl_box_int64(variable->integer());
    break;

  case BERTBuffers::Variable::kStr:
    value = jl_pchar_to_string(variable->str().c_str(), variable->str().length());
    break;

  case BERTBuffers::Variable::kComPointer:
  {
    const auto &com_pointer = variable->com_pointer();
    // std::cout << "installing external pointer: " << com_pointer.interface_name() << " @ " << std::hex << com_pointer.pointer() << std::endl;

    jl_value_t* array_type = jl_apply_array_type((jl_value_t*)jl_any_type, 1);
    jl_array_t* julia_array = jl_alloc_array_1d(array_type, 4);

    //jl_value_t *element = VariableToJlValue(&(arr.data(i)));
    //jl_arrayset(julia_array, element, i);

    // name
    jl_arrayset(julia_array,
      jl_pchar_to_string(com_pointer.interface_name().c_str(), com_pointer.interface_name().length()), 0);

    // pointer (literal)
    jl_arrayset(julia_array, jl_box_uint64(com_pointer.pointer()), 1);

    // functions
    if (com_pointer.functions_size()) {
      jl_array_t *functions_array = jl_alloc_array_1d(array_type, com_pointer.functions_size());
      for (int i = 0, len = com_pointer.functions_size(); i < len; i++) {
        const auto &function_definition = com_pointer.functions(i);

         // function definition should be name, type, index, [params]
        jl_array_t *function_def_array = jl_alloc_array_1d(array_type, 4);

        jl_arrayset(function_def_array, 
          jl_pchar_to_string(function_definition.function().name().c_str(), function_definition.function().name().length()), 0);

        std::string call_type = "method";
        switch (function_definition.call_type()) {
        case BERTBuffers::CallType::get: call_type = "get";
          break;
        case BERTBuffers::CallType::put: call_type = "put";
          break;
        }

        jl_arrayset(function_def_array, jl_pchar_to_string(call_type.c_str(), call_type.length()), 1);
        jl_arrayset(function_def_array, jl_box_uint32(function_definition.function().index()), 2);

        int arguments_count = function_definition.arguments_size();
        if (arguments_count > 0) {
          jl_array_t *arguments_array = jl_alloc_array_1d(array_type, arguments_count);
          for (int j = 0; j < arguments_count; j++) {
            const auto &argument = function_definition.arguments(j);
            jl_arrayset(arguments_array,
              jl_pchar_to_string(argument.name().c_str(), argument.name().length()), j);
          }
          jl_arrayset(function_def_array, (jl_value_t*)arguments_array, 3);
        }
        else {
          jl_arrayset(function_def_array, jl_nothing, 3);
        }

        jl_arrayset(functions_array, (jl_value_t*)function_def_array, i);
      }
      jl_arrayset(julia_array, (jl_value_t*)functions_array, 2);
    }
    else jl_arrayset(julia_array, jl_nothing, 2);

    // enums
    if (com_pointer.enums_size()) {
      jl_array_t *enums_array = jl_alloc_array_1d(array_type, com_pointer.enums_size());
      for (int i = 0, len = com_pointer.enums_size(); i < len; i++) {
        const auto &enum_definition = com_pointer.enums(i);
        jl_value_t *val = jl_pchar_to_string(enum_definition.name().c_str(), enum_definition.name().length());
        jl_arrayset(enums_array, val, i);
      }
      jl_arrayset(julia_array, (jl_value_t*)enums_array, 3);
    }
    else jl_arrayset(julia_array, jl_nothing, 3);

    value = (jl_value_t*)julia_array;

    break;
  }
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

      for (int i = 0; i < nrows; i++) {
        jl_value_t *element = VariableToJlValue(&(arr.data(i)));
        jl_arrayset(julia_array, element, i);
      }

    }
    else {

      jl_value_t* array_type = jl_apply_array_type((jl_value_t*)jl_any_type, 2);
      julia_array = jl_alloc_array_2d(array_type, nrows, ncols);

      for (int i = 0; i < ncols; i++) {
        for (int j = 0; j < nrows; j++) {
          jl_value_t *element = VariableToJlValue(&(arr.data(i)));
          jl_arrayset(julia_array, element, j + nrows * i);
        }
      }

    }
    
    value = (jl_value_t*)julia_array;
    break;
  }

  case 0: // not set (should be missing?)
    value = jl_nothing;
    break;

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
    variable->set_real(jl_unbox_float64(value));
    return;
  }
  if (jl_typeis(value, jl_float32_type)) {
    variable->set_real(jl_unbox_float32(value));
    return;
  }

  // ints
  if (jl_typeis(value, jl_int64_type)) {
    variable->set_integer(jl_unbox_int64(value));
    return;
  }
  if (jl_typeis(value, jl_int32_type)) {
    variable->set_integer(jl_unbox_int32(value));
    return;
  }
  if (jl_typeis(value, jl_uint32_type)) {
    variable->set_integer(jl_unbox_uint32(value));
    return;
  }
  if (jl_typeis(value, jl_uint64_type)) {
    variable->set_integer(jl_unbox_uint64(value));
    return;
  }
  if (jl_typeis(value, jl_int16_type)) {
    variable->set_integer(jl_unbox_int16(value));
    return;
  }
  if (jl_typeis(value, jl_int8_type)) {
    variable->set_integer(jl_unbox_int8(value));
    return;
  }

  // complex...

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
      for (int i = 0; i < len; i++) results_array->add_data()->set_real(d[i]);
      return;
    }
    else if (eltype == jl_float32_type) {
      float *d = (float*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_real(d[i]);
      return;
    }
    else if (eltype == jl_int64_type) {
      int64_t *d = (int64_t*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_integer(d[i]);
      return;
    }
    else if (eltype == jl_uint64_type) {
      uint64_t *d = (uint64_t*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_integer(d[i]);
      return;
    }
    else if (eltype == jl_int32_type) {
      int32_t *d = (int32_t*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_integer(d[i]);
      return;
    }
    else if (eltype == jl_int8_type) {
      int8_t *d = (int8_t*)jl_array_data(jl_array);
      for (int i = 0; i < len; i++) results_array->add_data()->set_integer(d[i]);
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

  /*
  
  if (jl_is_nothing(value)) { std::cout << "jl_is_nothing(value)" << std::endl; }
  if (jl_is_tuple(value)) { std::cout << "jl_is_tuple(value)" << std::endl; }
  if (jl_is_svec(value)) { std::cout << "jl_is_svec(value)" << std::endl; }
  if (jl_is_simplevector(value)) { std::cout << "jl_is_simplevector(value)" << std::endl; }
  if (jl_is_datatype(value)) { std::cout << "jl_is_datatype(value)" << std::endl; }
  if (jl_is_mutable(value)) { std::cout << "jl_is_mutable(t)" << std::endl; }
  if (jl_is_mutable_datatype(value)) { std::cout << "jl_is_mutable_datatype(t)" << std::endl; }
  if (jl_is_immutable(value)) { std::cout << "jl_is_immutable(t)" << std::endl; }
  if (jl_is_immutable_datatype(value)) { std::cout << "jl_is_immutable_datatype(t)" << std::endl; }
  if (jl_is_uniontype(value)) { std::cout << "jl_is_uniontype(value)" << std::endl; }
  if (jl_is_typevar(value)) { std::cout << "jl_is_typevar(value)" << std::endl; }
  if (jl_is_unionall(value)) { std::cout << "jl_is_unionall(value)" << std::endl; }
  if (jl_is_typename(value)) { std::cout << "jl_is_typename(value)" << std::endl; }
  if (jl_is_int8(value)) { std::cout << "jl_is_int8(value)" << std::endl; }
  if (jl_is_int16(value)) { std::cout << "jl_is_int16(value)" << std::endl; }
  if (jl_is_int32(value)) { std::cout << "jl_is_int32(value)" << std::endl; }
  if (jl_is_int64(value)) { std::cout << "jl_is_int64(value)" << std::endl; }
  if (jl_is_uint8(value)) { std::cout << "jl_is_uint8(value)" << std::endl; }
  if (jl_is_uint16(value)) { std::cout << "jl_is_uint16(value)" << std::endl; }
  if (jl_is_uint32(value)) { std::cout << "jl_is_uint32(value)" << std::endl; }
  if (jl_is_uint64(value)) { std::cout << "jl_is_uint64(value)" << std::endl; }
  if (jl_is_bool(value)) { std::cout << "jl_is_bool(value)" << std::endl; }
  if (jl_is_symbol(value)) { std::cout << "jl_is_symbol(value)" << std::endl; }
  if (jl_is_ssavalue(value)) { std::cout << "jl_is_ssavalue(value)" << std::endl; }
  if (jl_is_slot(value)) { std::cout << "jl_is_slot(value)" << std::endl; }
  if (jl_is_expr(value)) { std::cout << "jl_is_expr(value)" << std::endl; }
  if (jl_is_globalref(value)) { std::cout << "jl_is_globalref(value)" << std::endl; }
  if (jl_is_labelnode(value)) { std::cout << "jl_is_labelnode(value)" << std::endl; }
  if (jl_is_gotonode(value)) { std::cout << "jl_is_gotonode(value)" << std::endl; }
  if (jl_is_quotenode(value)) { std::cout << "jl_is_quotenode(value)" << std::endl; }
  if (jl_is_newvarnode(value)) { std::cout << "jl_is_newvarnode(value)" << std::endl; }
  if (jl_is_linenode(value)) { std::cout << "jl_is_linenode(value)" << std::endl; }
  if (jl_is_method_instance(value)) { std::cout << "jl_is_method_instance(value)" << std::endl; }
  if (jl_is_code_info(value)) { std::cout << "jl_is_code_info(value)" << std::endl; }
  if (jl_is_method(value)) { std::cout << "jl_is_method(value)" << std::endl; }
  if (jl_is_module(value)) { std::cout << "jl_is_module(value)" << std::endl; }
  if (jl_is_mtable(value)) { std::cout << "jl_is_mtable(value)" << std::endl; }
  if (jl_is_task(value)) { std::cout << "jl_is_task(value)" << std::endl; }
  if (jl_is_string(value)) { std::cout << "jl_is_string(value)" << std::endl; }
  if (jl_is_cpointer(value)) { std::cout << "jl_is_cpointer(value)" << std::endl; }
  if (jl_is_pointer(value)) { std::cout << "jl_is_pointer(value)" << std::endl; }
  if (jl_is_intrinsic(value)) { std::cout << "jl_is_intrinsic(value)" << std::endl; }

  */

  if (jl_is_cpointer(value)) {

    // where does it keep track of the underlying type? in my testing, all pointers 
    // irrespective of julia size (e.g. Ptr{UInt32}) report size = 8; so we can treat 
    // them as 64-bit pointers. but let's check anyway, just in case...

    jl_datatype_t* value_type = (jl_datatype_t*)(jl_typeof(value));
    if (value_type->size != 8) {
      std::cerr << "warning: pointer size not == 8 (" << value_type->size << ")" << std::endl;
    }
    else {
      auto com_pointer = variable->mutable_com_pointer();
      com_pointer->set_pointer(reinterpret_cast<uint64_t>(jl_unbox_voidpointer(value)));
      // std::cout << "read external pointer: " << std::hex << com_pointer->pointer() << std::endl;
      return;
    }

  }

  //////////////

  jl_value_t *value_type = jl_typeof(value);
  jl_printf(JL_STDOUT, "unexpected type (0x%X): ", value_type);
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

  // this is used for tagging

  static char filename[] = "shell";
  static int filename_len = (int)strlen(filename);

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

// testing
extern void DumpJSON(const google::protobuf::Message &message, const char *path);

extern bool Callback(const BERTBuffers::CallResponse &call, BERTBuffers::CallResponse &response);

jl_value_t* Callback2(const char *command, void *data) {

  // FIXME: unify callback functions

  /*
  std::cout << "CB2: " << command << std::endl;
  if (data) {
    BERTBuffers::Variable var;
    JlValueToVariable(&var, reinterpret_cast<jl_value_t*>(data), true);
    DumpJSON(var, 0);
  }
  */
  
  static uint32_t callback_id = 1;
  jl_value_t *jl_result = jl_nothing;

  if (!command || !command[0]) return jl_result;

  BERTBuffers::CallResponse *call = new BERTBuffers::CallResponse;
  BERTBuffers::CallResponse *response = new BERTBuffers::CallResponse;

  call->set_id(callback_id++);
  call->set_wait(true);

  auto callback = call->mutable_function_call();
  callback->set_function(command);

  if (data) //SEXPToVariable(callback->add_arguments(), reinterpret_cast<SEXP>(data));
    JlValueToVariable(callback->add_arguments(), reinterpret_cast<jl_value_t*>(data));

  bool success = Callback(*call, *response);
  // cout << "callback (2) complete (" << success << ")" << endl;

  if (success) jl_result = VariableToJlValue(&(response->result()));

  delete call;
  delete response;

  if (!success) {
    // error_return("internal method failed");
  }
  
  return jl_result;

}

jl_value_t * COMCallback(uint64_t pointer, const char *name, const char *calltype, uint32_t index, void *arguments_list) {

  /*

  std::cout << "CB1: " << pointer << ", " << name << ", " << calltype << ", " << index << std::endl;
  if (arguments_list) {
    BERTBuffers::Variable var;
    JlValueToVariable(&var, (jl_value_t*)arguments_list, true);
    DumpJSON(var, 0);
  }
  return jl_box_int32(44);
//  return jl_nothing;

  */
  
  static uint32_t callback_id = 1;
  jl_value_t *jl_result = jl_nothing;

  if (!name || !name[0]) return jl_result;

  BERTBuffers::CallResponse *call = new BERTBuffers::CallResponse;
  BERTBuffers::CallResponse *response = new BERTBuffers::CallResponse;

  call->set_id(callback_id++);
  call->set_wait(true);

  //auto callback = call->mutable_com_callback();
  auto callback = call->mutable_function_call();
  callback->set_target(BERTBuffers::CallTarget::COM);
  callback->set_function(name);

  callback->set_index(index);
  callback->set_pointer(pointer);

  callback->set_type(BERTBuffers::CallType::method);
  if (calltype) {
    if (!strcmp(calltype, "get")) callback->set_type(BERTBuffers::CallType::get);
    else if (!strcmp(calltype, "put")) callback->set_type(BERTBuffers::CallType::put);
  }

  if (arguments_list) {
    BERTBuffers::Variable var;
    JlValueToVariable(&var, (jl_value_t*)arguments_list);
    int len = var.arr().data_size();
    for (int i = 0; i < len; i++) {
      callback->add_arguments()->CopyFrom(var.arr().data(i)); // FIXME
    }
    //std::cout << "arguments list:" << std::endl;
    //DumpJSON(var, 0);
  }

  bool success = Callback(*call, *response);

  if (success) {
    int err = 0;
    if (response->operation_case() == BERTBuffers::CallResponse::OperationCase::kFunctionCall) {

      // sexp_result = RCallSEXP(response->function_call(), true, err); // ??
      jl_result = jl_box_int32(100);
    }
    else if (response->operation_case() == BERTBuffers::CallResponse::OperationCase::kResult
      && response->result().value_case() == BERTBuffers::Variable::ValueCase::kComPointer) {

      jl_result = jl_box_int32(200);

      BERTBuffers::CompositeFunctionCall *function_call = new BERTBuffers::CompositeFunctionCall;
      function_call->set_function("BERT.CreateCOMType");
      auto argument = function_call->add_arguments();

      // I want to borrow this, not copy, can we do that?
      argument->mutable_com_pointer()->CopyFrom(response->result().com_pointer());

      //sexp_result = RCallSEXP(*function_call, true, err);
      //argument->release_com_pointer();

      jl_result = JuliaCallJlValue(*function_call);

      delete function_call;
      //*/
    }
    else jl_result = VariableToJlValue(&(response->result()));
  }

  delete call;
  delete response;

  if (!success) {
    jl_printf(JL_STDERR, "Error in COM call (...)\n");
  }

  return jl_result;
}


jl_function_t* FindFunction(const std::string &function) {

  jl_function_t *function_pointer = jl_nothing;

  if (!function.length()) return function_pointer;
  std::vector<std::string> elements;
  StringUtilities::Split(function, '.', 1, elements);

  if (elements.size() == 1) function_pointer = jl_get_function(jl_main_module, function.c_str());
  else {
    function_pointer = jl_get_global(jl_main_module, jl_symbol(elements[0].c_str()));
    for (int i = 1; i< elements.size(); i++) function_pointer = jl_get_global((jl_module_t*)function_pointer, jl_symbol(elements[i].c_str()));
  }

  return function_pointer;

}

__inline bool ReportException(const char *tag) {

  // ASSERT(message); // not zero

  if (jl_exception_occurred()) {
    std::cout << "* [" << tag << "] EXCEPTION" << std::endl;
    jl_printf(JL_STDERR, "[%s] error during run:\n", tag);
    jl_static_show(JL_STDERR, ptls->exception_in_transit);
    jl_exception_clear();
    jl_printf(JL_STDOUT, "\n");

    // set err
    //response.set_err("julia exception");
    
    return true;
  }
  return false;
}

bool JuliaPostInit() {
  
  jl_function_t *function_pointer = FindFunction("BERT.SetCallbacks");
  if (!function_pointer || jl_is_nothing(function_pointer)) return false;

  std::vector<jl_value_t*> arguments_vector = {
    jl_box_voidpointer(&COMCallback),
    jl_box_voidpointer(&Callback2)
  };
  
  JL_TRY{
    jl_call(function_pointer, &(arguments_vector[0]), arguments_vector.size());
    ReportException("post-install");
  }
  JL_CATCH{
    std::cout << "* CATCH [post-install]" << std::endl;
    jl_printf(JL_STDERR, "\nparser error:\n");
    jl_static_show(JL_STDERR, ptls->exception_in_transit);
    jl_printf(JL_STDERR, "\n");
    jlbacktrace();
    jl_exception_clear();
  }

  /*

  auto mutable_function_call = translated_call.mutable_function_call();
  mutable_function_call->set_target(BERTBuffers::CallTarget::language);
  mutable_function_call->set_function("BERT.SetCallbacks");
  mutable_function_call->add_arguments()->set_u64((uint64_t)&COMCallback);
  mutable_function_call->add_arguments()->set_u64((uint64_t)&Callback2);
  JuliaCall(response, translated_call);

  */

  return true;
}

jl_value_t *JuliaCallJlValue(const BERTBuffers::CompositeFunctionCall &call) {

  jl_function_t *function_pointer = FindFunction(call.function());
  jl_value_t *function_result = jl_nothing;
  if (!function_pointer || jl_is_nothing(function_pointer)) return function_result;

  JL_TRY{

    // call here, with or without arguments
    int len = call.arguments().size();
    if (len > 0) {
      std::vector<jl_value_t*> arguments_vector;
      for (auto argument : call.arguments()) {
        arguments_vector.push_back(VariableToJlValue(&argument));
      }
      function_result = jl_call(function_pointer, &(arguments_vector[0]), len);
    }
    else {
      function_result = jl_call0(function_pointer);
    }
    ReportException("JCJV");
     
  }
  JL_CATCH{

    // external exception; return error

    std::cout << "* CATCH [JCJV]" << std::endl;
    jl_printf(JL_STDERR, "\nparser error:\n");

    jl_static_show(JL_STDERR, ptls->exception_in_transit);
    jl_printf(JL_STDERR, "\n");
    jlbacktrace();
    jl_exception_clear();

   //response.set_err("external exception");

  }

  return function_result;
}

void JuliaCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call) {

  response.set_id(call.id());

  std::string function = call.function_call().function();

  // lookup in main includes our defined functions plus (apparently) Base, which 
  // is attached is some fashion. can we dereference pacakges? [A: no, probably 
  // need to do that manually]. [moved to function]

  jl_function_t *function_pointer = FindFunction(function);
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

inline std::string jl_string(jl_value_t *value){ return std::string(jl_string_ptr(value), jl_string_len(value)); }

void ListScriptFunctions(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call) {
  
  bool success = false;
  response.set_id(call.id());
  
  // FIXME: wrap in try-catch? might be easier to handle 

  std::string command = "BERT.ListFunctions()"; // newline?

  JL_TRY{

    jl_value_t *val = (jl_value_t*)jl_load_file_string(command.c_str(), command.length(), "inline (list-functions)");

    if (jl_exception_occurred()) {
      std::cout << "* [LSF] EXCEPTION" << std::endl;
      jl_exception_clear();
    }
    else {
      success = true;
      auto function_list = response.mutable_function_list();

      auto ParseEntry = [function_list](jl_value_t * element){

        // function descriptions are lists of function name, [arg name]
        // FIXME/TODO: get (at least) docs in there

        if (jl_is_array(element)) {

          auto jl_array = (jl_array_t*)element;
          auto eltype = jl_array_eltype(element);
          auto array_length = jl_array->length;

          // expect string[], don't handle other types...

          if (array_length == 0) {
            std::cout << "function list entry has length 0" << std::endl;
          }
          else if (eltype == jl_string_type) {
            auto data = (jl_value_t**)(jl_array_data(jl_array));
            auto function_descriptor = function_list->add_functions();

            // FIXME: any other metadata

            function_descriptor->mutable_function()->set_name(jl_string(data[0]));
            for (int i = 1; i < array_length; i++) {
              function_descriptor->add_arguments()->set_name(jl_string(data[i]));
            }

          }
          else {
            std::cout << "function list entry, unexpected type: " << eltype << " (array flag? " << (jl_array->flags.ptrarray ? "true" : "false") << ")" << std::endl;
          }

          /*
          std::cout << "entry length: " << array_length << std::endl;

          if (eltype == jl_string_type) {
            std::cout << "\ttype is string" << std::endl;
          }
          else if (jl_array->flags.ptrarray) {
            std::cout << "\tarray flag" << std::endl;
          }
          else {
            std::cout << "\tother type? " << eltype << std::endl;
          }
          */

        }
      };

      if (val) {

        // return value is an array of function descriptions. 

        if (jl_is_array(val)) {

          auto jl_array = (jl_array_t*)val;
          auto eltype = jl_array_eltype(val); 
          auto array_length = jl_array->length;

          if (jl_array->flags.ptrarray) {
            auto data = (jl_value_t**)(jl_array_data(jl_array));
            for (int i = 0; i < array_length; i++) {
              // JlValueToVariable(results_array->add_data(), data[i]);
              ParseEntry(data[i]);
            }
            return;
          }

        }

      }
      else {
        // success, but list length is zero
        // ...
      }
    }

  }
  JL_CATCH {

    std::cout << "* CATCH [LSF]" << std::endl;
    jl_printf(JL_STDERR, "\nparser error:\n");
    jl_static_show(JL_STDERR, ptls->exception_in_transit);
    jl_printf(JL_STDERR, "\n");
    jlbacktrace();
    jl_exception_clear();

  }

  if (!success) {
    response.set_err("error listing functions");
  }

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

