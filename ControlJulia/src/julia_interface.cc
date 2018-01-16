
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

#define JULIA_ENABLE_THREADING 1

#include "julia.h"

jl_ptls_t ptls;

jl_value_t * VariableToJlValue(BERTBuffers::Variable *variable) {

  switch (variable->value_case()) {
  case BERTBuffers::Variable::kBoolean:
    return jl_box_bool(variable->boolean());

  case BERTBuffers::Variable::kNum:
    return jl_box_float64(variable->num());

  case BERTBuffers::Variable::kStr:
    return jl_pchar_to_string(variable->str().c_str(), variable->str().length());

  default:
    std::cout << "Unhandled type in variable to jlvalue: " << variable->value_case() << std::endl;
  }

  return jl_nothing;
  
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

  // array

  if (jl_is_array(value)) {

    auto results_array = variable->mutable_arr();
    auto jl_array = (jl_array_t*)value;
    auto eltype = jl_array_eltype(value);

    int ncols = jl_array->ncols;
    int nrows = jl_array->nrows;
    int len = jl_array->length;

    std::cout << "arr: " << ncols << ", " << nrows << ", " << len << "; p? " << (jl_array->flags.ptrarray != 0) << std::endl;

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
  jl_eval_string("Base.banner()");

}

void JuliaShutdown() {

  /* strongly recommended: notify Julia that the
  program is about to terminate. this allows
  Julia time to cleanup pending write requests
  and run all finalizers
  */
  jl_atexit_hook(0);

}

/**
 * shell exec: response is printed to output stream
 */
void julia_exec_command(const std::string &command) {

  JL_TRY{
    jl_value_t *val = (jl_value_t*)jl_eval_string(command.c_str());
    if (jl_exception_occurred()) {
      std::cout << "* EXCEPTION" << std::endl;

      jl_printf(JL_STDERR, "error during run:\n");
      jl_static_show(JL_STDERR, ptls->exception_in_transit);
      jl_exception_clear();
    }
    else if (val) {
      jl_static_show(JL_STDOUT, val);
    }
    jl_printf(JL_STDOUT, "\n");
  }
    JL_CATCH{
      std::cout << "* CATCH" << std::endl;

      jl_printf(JL_STDERR, "\nparser error:\n");
      jl_static_show(JL_STDERR, ptls->exception_in_transit);
      jl_printf(JL_STDERR, "\n");
      jlbacktrace();
  }

  int remaining_handles = uv_run(jl_global_event_loop(), UV_RUN_NOWAIT);
  while (remaining_handles) {
    remaining_handles = uv_run(jl_global_event_loop(), UV_RUN_ONCE);
  }

  // note this does not expect a newline/cr
//  jl_eval_string(command.c_str());

}

void JuliaCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call) {

  response.set_id(call.id());

  std::string function = call.function_call().function();

  // lookup in main includes our defined functions plus (apparently) Base, which 
  // is attached is some fashion. can we dereference pacakges? [A: no, probably 
  // need to do that manually].

  jl_function_t *function_pointer = jl_get_function(jl_main_module, function.c_str());
  
  if (!function_pointer || jl_is_nothing(function_pointer)) return;
  
  jl_value_t *function_result;

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

  JlValueToVariable(response.mutable_result(), function_result);

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

    if (jl_exception_occurred()) {
      std::cout << "* EXCEPTION" << std::endl;
      jl_printf(JL_STDERR, "error during run:\n");
      jl_static_show(JL_STDERR, ptls->exception_in_transit);
      jl_exception_clear();
    }
    else if (val) {
      JlValueToVariable(response.mutable_result(), val);
      //jl_static_show(JL_STDOUT, val);
    }
    //jl_printf(JL_STDOUT, "\n");
  }
  JL_CATCH {
    std::cout << "* CATCH" << std::endl;
    jl_printf(JL_STDERR, "\nparser error:\n");
    jl_static_show(JL_STDERR, ptls->exception_in_transit);
    jl_printf(JL_STDERR, "\n");
    jlbacktrace();
  }


}

