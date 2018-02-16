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
//typedef SEXP CALLBACK_FN( int, void*, void* );

// new style
typedef SEXP CALLBACK_FUNCTION( SEXP, SEXP );

SEXP Callback2(SEXP command, SEXP data){
  static CALLBACK_FUNCTION *function = (CALLBACK_FUNCTION*)R_GetCCallable("BERTControlR", "Callback");
  if(function) return function(command, data);
  else return R_NilValue;
}

/*
SEXP callback( int cmd, void * data, void * data2 ){
  static CALLBACK_FN *fn = (CALLBACK_FN*)R_GetCCallable( "BERTControlR", "ExternalCallback" );
  if( fn ) return fn( cmd, data, data2 );
  else return R_NilValue;
}
*/

/**
 * constructor, sets default options.  see [1]
 */
pDevDesc device_new( rcolor bg, double width, double height, int pointsize) {

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

    // straight up from [1]
    
    dd->cra[0] = 0.9 * pointsize;
    dd->cra[1] = 1.2 * pointsize;
    dd->xCharOffset = 0.4900;
    dd->yCharOffset = 0.3333;
    dd->yLineBias = 0.2;
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

//=============================================================================
//
// exported functions 
//
//=============================================================================

/**
 * create graphics device, add to display list 
 */ 
int create_device( std::string name, std::string background, int width, int height, int pointsize) 
{
    rcolor bg = R_GE_str2col(background.c_str());
    int device = 0;
  
    R_GE_checkVersionOrDie(R_GE_version);
    R_CheckDeviceAvailable();

    pDevDesc dev = device_new(bg, width, height, pointsize);
    //callback( 1, (void*)name.c_str(), dev );
    Callback2(Rf_mkString("create-device"), R_MakeExternalPtr(dev, R_NilValue, R_NilValue)); 

    pGEDevDesc gd = GEcreateDevDesc(dev);
    GEaddDevice2(gd, name.c_str());
    GEinitDisplayList(gd);
    device = GEdeviceNumber(gd) + 1; // to match what R says

//    ((JSGraphicsDevice*)(dev->deviceSpecific))->setDevice(device);
    cout << "device number: " << device << endl;

    return device;
  
}

/**
 * create console graphics device, add to display list 
 */ 
int create_console_device( std::string background, int width, int height, int pointsize) 
{
    rcolor bg = R_GE_str2col(background.c_str());
    int device = 0;
  
    R_GE_checkVersionOrDie(R_GE_version);
    R_CheckDeviceAvailable();

    pDevDesc dev = device_new(bg, width, height, pointsize);
    Callback2(Rf_mkString("console-device"), R_MakeExternalPtr(dev, R_NilValue, R_NilValue)); 

    pGEDevDesc gd = GEcreateDevDesc(dev);
    GEaddDevice2(gd, "BERT-console-graphics-device");
    GEinitDisplayList(gd);
    device = GEdeviceNumber(gd) + 1; // to match what R says

    return device;
  
}

/**
 * release the pointer. this requires a callback to the actual owner,
 * and we don't do any validation. legal to be zero? that just seems 
 * like a problem waiting to happen... so no.
 */
void ReleasePointer(SEXP pointer) {
  int key = (int)((intptr_t)R_ExternalPtrAddr(pointer));
	if (key) {
    Callback2(Rf_mkString("release-pointer"), Rf_ScalarInteger(key));
	}
}

/**
 * create an external pointer and register a finalizer. key is not necessarily
 * the pointer address, it could use a mapping. although for practical purposes
 * I think it will usually be the pointer address.
 *
 * actually, that's not correct: because R only has 32-bit integers, it might
 * be easier to use a map. if we're getting called from R, anything else will
 * require constructing and destructing 64-bit values from lists, like we do
 * in the excel reference type. using a map may  be preferable.
 *
 * although this requires some nasty casting.
 */
SEXP InstallPointer(SEXP key){
  SEXP pointer = R_MakeExternalPtr((void*)((intptr_t)(Rf_asInteger(key))), install("COM dispatch pointer"), R_NilValue);
	R_RegisterCFinalizerEx(pointer, (R_CFinalizer_t)ReleasePointer, TRUE);
  return pointer;
}

SEXP BERTModule_COM_Callback( SEXP name, SEXP calltype, SEXP index, SEXP p, SEXP args ){
  static COM_CALLBACK_FN *fn = (COM_CALLBACK_FN*)R_GetCCallable( "BERTControlR", "COMCallback" );
  if( fn ) return fn( name, calltype, index, p, args );
  else return R_NilValue;
}

SEXP BERTModule_create_device( SEXP name, SEXP background, SEXP width, SEXP height, SEXP pointsize ){
  int dev = create_device( CHAR(STRING_ELT(name, 0)), CHAR(STRING_ELT(background, 0)),
    Rf_asReal( width ), Rf_asReal( height ), Rf_asReal( pointsize ));
  return Rf_ScalarInteger(dev);
}

SEXP BERTModule_console_device( SEXP background, SEXP width, SEXP height, SEXP pointsize ){
  int dev = create_console_device(CHAR(STRING_ELT(background, 0)),
    Rf_asReal( width ), Rf_asReal( height ), Rf_asReal( pointsize ));
  return Rf_ScalarInteger(dev);
}

/*
SEXP BERTModule_download( SEXP args ){
  SEXP s = callback( 100, args, 0 );
  int result = Rf_asInteger(s);
  if( result < 0 ) Rf_error("Download failed");
  return s;
}

SEXP BERTModule_progress_bar( SEXP args ){
  return callback( 200, args, 0 );
}
*/

void BERTModule_close_console(){
  // callback( 300, 0, 0 );
  Callback2(Rf_mkString("close-console"), 0);
}

SEXP BERTModule_history( SEXP args ){
  //return callback( 400, args, 0 );
  return Callback2(Rf_mkString("console-history"), args);
}

SEXP BERTModule_Callback(SEXP command, SEXP data){
  return Callback2(command, data);
}

extern "C" {
    void R_init_BERTModule(DllInfo *info)
    {
        static R_CallMethodDef methods[]  = {
          //  { "create_device", (DL_FUNC)&BERTModule_create_device, 5},
            { "console_device", (DL_FUNC)&BERTModule_console_device, 4},
          //  { "progress_bar", (DL_FUNC)&BERTModule_progress_bar, 1},
          //  { "download", (DL_FUNC)&BERTModule_download, 1},
            { "history", (DL_FUNC)&BERTModule_history, 1},
            { "InstallPointer", (DL_FUNC)&InstallPointer, 1},
            { "Callback", (DL_FUNC)&BERTModule_Callback, 2},
            { "COMCallback", (DL_FUNC)&BERTModule_COM_Callback, 5},
            { "close_console", (DL_FUNC)&BERTModule_close_console, 0},
            { NULL, NULL, 0 }
        };
        R_registerRoutines( info, NULL, methods, NULL, NULL);
    }
}
