/**
 * Copyright (c) 2016 Structured Data LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * The structure and some of the methods in this file are based on 
 *
 * https://github.com/hadley/svglite
 *
 * which is copyright
 *
 *  (C) 2002 T Jake Luciani: SVG device, based on PicTex device
 *  (C) 2008 Tony Plate: Line type support from RSVGTipsDevice package
 *  (C) 2012 Matthieu Decorde: UTF-8 support, XML reserved characters and XML header
 *  (C) 2015 RStudio (Hadley Wickham): modernisation & refactoring
 *
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

typedef SEXP COM_CALLBACK_FN( SEXP, SEXP, SEXP, SEXP, SEXP );
typedef SEXP CALLBACK_FUNCTION( SEXP, SEXP );

SEXP Callback2(SEXP command, SEXP data){
  static CALLBACK_FUNCTION *function = (CALLBACK_FUNCTION*)R_GetCCallable("BERTControlR", "Callback");
  if(function) return function(command, data);
  else return R_NilValue;
}

/**
 * constructor, sets default options.  see [1]
 */
pDevDesc device_new( rcolor bg, double width, double height, int pointsize ) {

    // this is the only part we need in the module. everything
    // else can move to the implementation.

    pDevDesc dd = (DevDesc*) calloc(1, sizeof(DevDesc));
    if (dd == NULL) return dd;
   
    dd->startfill = bg;
    dd->startcol = R_RGB(0, 0, 0);
    dd->startps = pointsize;
    dd->startlty = 0;
    dd->startfont = 1;
    dd->startgamma = 1;
    dd->wantSymbolUTF8 = (Rboolean) 1;
    dd->hasTextUTF8 = (Rboolean) 1;

    dd->left = 0;
    dd->top = 0;
    dd->right = width;
    dd->bottom = height;

    // straight up from [1]. these are "character size in rasters", 
    // whatever that means.
    
    dd->cra[0] = 0.9 * pointsize;
    dd->cra[1] = 1.2 * pointsize;
    //dd->cra[1] = 1.1 * dd->startps; // ??
   
    // according to 
    // include\R_ext\GraphicsDevice.h
    // xCharOffset is unused. not sure what that means for the others.

    dd->xCharOffset = 0.4900;
    dd->yCharOffset = 0.3333;
    dd->yLineBias = 0.2;

    // inches per raster; this should change for high DPI screens

    dd->ipr[0] = 1.0 / 72.0;
    dd->ipr[1] = 1.0 / 72.0;

    dd->canClip = FALSE;
    dd->canHAdj = 0;
    dd->canChangeGamma = FALSE;
    dd->displayListOn = TRUE;
    dd->haveTransparency = 2;
    dd->haveTransparentBg = 2;

    dd->haveRaster = 3; // yes, except for missing values

    return dd;
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
 * create graphics device, add to display list 
 * / 
int create_device( std::string name, std::string background, int width, int height, int pointsize) 
{
    rcolor bg = R_GE_str2col(background.c_str());
    int device = 0;
  
    R_GE_checkVersionOrDie(R_GE_version);
    R_CheckDeviceAvailable();

    pDevDesc dev = device_new(bg, width, height, pointsize);
    Callback2(Rf_mkString("create-device"), R_MakeExternalPtr(dev, R_NilValue, R_NilValue)); 

    pGEDevDesc gd = GEcreateDevDesc(dev);
    GEaddDevice2(gd, name.c_str());
    GEinitDisplayList(gd);
    device = GEdeviceNumber(gd) + 1; // to match what R says

    cout << "device number: " << device << endl;

    return device;
  
}
*/

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

  SEXP device = Callback2(Rf_mkString("spreadhseet-device"), argument_list); 

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
