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
 
#include <Rconfig.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <R_ext/GraphicsEngine.h>

#ifdef length
#undef length
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include <iostream>
#include <sstream>
#include <string>

using namespace std;

typedef SEXP CALLBACK_FUNCTION( SEXP, SEXP );

SEXP Callback2(SEXP command, SEXP data){
  static CALLBACK_FUNCTION *function = (CALLBACK_FUNCTION*)R_GetCCallable("BERTControlR", "Callback");
  if(function) return function(command, data);
  else return R_NilValue;
}

/**
 * constructor
 */
pDevDesc device_new_simple() {

    // this is the only part we need in the module. everything
    // else can move to the implementation.

    pDevDesc dd = (DevDesc*) calloc(1, sizeof(DevDesc));
    return dd;

}

//=============================================================================
//
// exported functions 
//
//=============================================================================

/**
 * creates and initializes console graphics device. parameters 
 * are set in the control process, we just need to do memory 
 * allocations here.
 *
 * FIXME: maybe do some advance parameter validation?
 */ 
SEXP CreateConsoleDevice(SEXP background, SEXP width, SEXP height, SEXP pointsize, SEXP type){

  R_GE_checkVersionOrDie(R_GE_version);

  // this will raise an error. there's an alternative which 
  // returns boolean, which might allow more graceful failure.

  R_CheckDeviceAvailable(); 

  // create the device. we'll need to wrap this in an EXTPTR 
  // struct in case it's a 64 bit pointer

  pDevDesc dev = device_new_simple(); // bg, width, height, pointsize);
  if(!dev) return R_NilValue;

  // this is another calloc we need to leave on this 
  // side of the wall

  pGEDevDesc gd = GEcreateDevDesc(dev);

  SEXP argument_list = PROTECT(Rf_allocVector(VECSXP, 6));
  SET_VECTOR_ELT(argument_list, 0, background);
  SET_VECTOR_ELT(argument_list, 1, width);
  SET_VECTOR_ELT(argument_list, 2, height);
  SET_VECTOR_ELT(argument_list, 3, pointsize);
  SET_VECTOR_ELT(argument_list, 4, type);

  // note we're now passing the pGEDevDesc instead of the inner 
  // pDevDesc, because we need both and we can access the one 
  // from the other

  SET_VECTOR_ELT(argument_list, 5, PROTECT(R_MakeExternalPtr(gd, R_NilValue, R_NilValue)));

  SEXP device = Callback2(Rf_mkString("console-device"), argument_list); 

  UNPROTECT(2);

  return device;
  
}

/**
 * create spreadsheet device, based on the new console device. 
 * doesn't take a type, but takes a name so we can identify it
 */ 
SEXP CreateSpreadsheetDevice(SEXP name, SEXP background, SEXP width, SEXP height, SEXP pointsize){

  R_GE_checkVersionOrDie(R_GE_version);

  // this will raise an error. there's an alternative which 
  // returns boolean, which might allow more graceful failure.

  R_CheckDeviceAvailable(); 

  // create the device. we'll need to wrap this in an EXTPTR 
  // struct in case it's a 64 bit pointer

  pDevDesc dev = device_new_simple(); // bg, width, height, pointsize);
  if(!dev) return R_NilValue;

  // this is another calloc we need to leave on this 
  // side of the wall

  pGEDevDesc gd = GEcreateDevDesc(dev);

  SEXP argument_list = PROTECT(Rf_allocVector(VECSXP, 6));
  SET_VECTOR_ELT(argument_list, 0, name);
  SET_VECTOR_ELT(argument_list, 1, background);
  SET_VECTOR_ELT(argument_list, 2, width);
  SET_VECTOR_ELT(argument_list, 3, height);
  SET_VECTOR_ELT(argument_list, 4, pointsize);

  // note we're now passing the pGEDevDesc instead of the inner 
  // pDevDesc, because we need both and we can access the one 
  // from the other

  SET_VECTOR_ELT(argument_list, 5, PROTECT(R_MakeExternalPtr(gd, R_NilValue, R_NilValue)));

  SEXP device = Callback2(Rf_mkString("spreadsheet-device"), argument_list); 

  UNPROTECT(2);

  return device;
  
}

void CloseConsole(){
  Callback2(Rf_mkString("close-console"), 0);
}

SEXP History( SEXP args ){
  return Callback2(Rf_mkString("console-history"), args);
}

extern "C" {
  void R_init_BERTModule(DllInfo *info)
  {
    static R_CallMethodDef methods[]  = {
      { "spreadsheet_device", (DL_FUNC)&CreateSpreadsheetDevice, 5},
      { "console_device", (DL_FUNC)&CreateConsoleDevice, 5},
      { "history", (DL_FUNC)&History, 1},
      { "close_console", (DL_FUNC)&CloseConsole, 0},
      { NULL, NULL, 0 }
    };
    R_registerRoutines( info, NULL, methods, NULL, NULL);
  }
}
