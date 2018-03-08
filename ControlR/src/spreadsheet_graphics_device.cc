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
#include "console_graphics_device.h"
#include "gdi_graphics_device.h"

// FIXME: some of the R internals use functions that windows declares
// deprecated for security. move this into an isolated lib so we don't
// have to deal with that.

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// we may be able to redefine these interfaces, as long as they're the
// same shape. probably fragile but still maybe preferable.

#include <R_ext/GraphicsEngine.h>

#ifdef length
#undef length
#endif

namespace SpreadsheetGraphicsDevice {

  std::vector< gdi_graphics_device::Device* > device_list;

  // callbacks: implemented

  void CloseDevice(pDevDesc dd) {

    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    if (device) {
      // remove from list
      std::vector<gdi_graphics_device::Device*> tmp;
      for (auto item : device_list) {
        if (item != device) tmp.push_back(item);
      }
      device_list = tmp;
      delete device;
    }
    dd->deviceSpecific = 0;
  }
  
  void NewPage(const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);

    device->UpdateSize();
    dd->right = dd->left + device->width();
    dd->bottom = dd->top + device->height();

    device->NewPage(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), dd->right, dd->bottom, dd->startfill );
  }

  void DrawLine(double x1, double y1, double x2, double y2, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawLine(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), x1, y1, x2, y2);
  }

  void DrawRect(double x1, double y1, double x2, double y2, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawRect(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), x1, y1, x2, y2);
  }

  void DrawCircle(double x, double y, double r, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawCircle(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), x, y, r);
  }
  
  void DrawPoly(int n, double *x, double *y, bool filled, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawPoly(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), n, x, y, filled);
  }

  void DrawPolyline(int n, double *x, double *y, const pGEcontext gc, pDevDesc dd) {
    DrawPoly(n, x, y, false, gc, dd);
  }

  void DrawPolygon(int n, double *x, double *y, const pGEcontext gc, pDevDesc dd) {
    DrawPoly(n, x, y, true, gc, dd);
  }

  void GetSize(double *left, double *right, double *bottom, double *top, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);

    // it makes sense that this would be called before new page, but it's not; 
    // not sure if anyone ever calls this function. we'll need to update size before 
    // new page?

    // actually ggplot2 calls it all the time. we _don't_ want to udpate here.

    //std::cout << "graphics get_size" << std::endl;
    //device->UpdateSize();

    // right/bottom will get set on new page

    *left = dd->left;
    *top = dd->top;
    *right = dd->right; // = (dd->left + device->width());
    *bottom = dd->bottom; // = (dd->top + device->height());
  }

  double GetStringWidth(const char *str, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    double width, height;
    device->MeasureText(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), str, &width, &height);
    return width;
  }

  void RenderText(double x, double y, const char *str, double rot, double hadj, const pGEcontext gc, pDevDesc dd) {

    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    if (hadj) {
      std::cerr << "WARNING: hadj " << hadj << std::endl;
    }
    device->RenderText(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), str, x, y, rot);
    
  }
  
  void GetMetricInfo(int c, const pGEcontext gc, double* ascent, double* descent, double* width, pDevDesc dd) {

    static char str[8];

    // this one can almost certainly do some caching.
    // Convert to string - negative implies unicode code point
    if (c < 0) {
      Rf_ucstoutf8(str, (unsigned int)-c);
    }
    else {
      str[0] = (char)c;
      str[1] = 0;
    }

    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->FontMetrics(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), str, ascent, descent, width);

    // std::cout << "text ``" << str << "'', ascent " << std::dec << *ascent << ", descent " << std::dec << *descent << std::endl;

  }

  void DrawRaster(unsigned int *raster,
    int pixel_width, int pixel_height,
    double x, double y,
    double target_width, double target_height,
    double rot,
    Rboolean interpolate,
    const pGEcontext gc, pDevDesc dd) {

    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawBitmap(raster, pixel_width, pixel_height, x, y, target_width, target_height, rot);

  }

  // callbacks: not implemented

  void DrawPath(double *x, double *y, int npoly, int *nper, Rboolean winding, const pGEcontext gc, pDevDesc dd) {
    std::cerr << "ENOTIMPL: draw_path" << std::endl; // FIXME
  }

  void SetClip (double x0, double x1, double y0, double y1, pDevDesc dd) {}

  // end callbacks

  SEXP CreateSpreadsheetDevice(const std::string &name, const std::string &background, double width, double height, double pointsize, void * pointer) {

    pGEDevDesc gd = (pGEDevDesc)pointer;
    pDevDesc dd = gd->dev;

    dd->startfill = R_GE_str2col(background.c_str());
    dd->startcol = R_RGB(0, 0, 0);
    dd->startps = pointsize;
    dd->startlty = 0;
    dd->startfont = 1;
    dd->startgamma = 1;
    dd->wantSymbolUTF8 = (Rboolean)1;
    dd->hasTextUTF8 = (Rboolean)1;

    // size

    dd->left = 0;
    dd->top = 0;
    dd->right = width;
    dd->bottom = height;

    // straight up from [1]. these are "character size in rasters", whatever that means.

    dd->cra[0] = 0.9 * pointsize;
    dd->cra[1] = 1.2 * pointsize;

    // according to 
    // include\R_ext\GraphicsDevice.h
    // xCharOffset is unused. not sure what that means for the others.

    dd->xCharOffset = 0.4900;
    dd->yCharOffset = 0.3333;
    dd->yLineBias = 0.2;

    // inches per raster; this should change for high DPI screens

    dd->ipr[0] = 1.0 / 72.0;
    dd->ipr[1] = 1.0 / 72.0;

    dd->canClip = (Rboolean)FALSE;
    dd->canHAdj = 0;
    dd->canChangeGamma = (Rboolean)FALSE;
    dd->displayListOn = (Rboolean)TRUE;
    dd->haveTransparency = 2;
    dd->haveTransparentBg = 2;
    dd->haveRaster = 3; // yes, except for missing values

    // std::cout << "init device (" << name << "): " << std::dec << dd->right << ", " << dd->bottom << std::endl;
    gdi_graphics_device::Device *device = new gdi_graphics_device::Device(name, width, height);
    
    device_list.push_back(device);

    dd->deviceSpecific = device;
    dd->newPage = &NewPage;
    dd->close = &CloseDevice;
    dd->line = &DrawLine;
    dd->rect = &DrawRect;
    dd->circle = &DrawCircle;

    dd->clip = &SetClip;
    dd->size = &GetSize;
    dd->metricInfo = &GetMetricInfo;
    dd->strWidth = &GetStringWidth;
    dd->text = &RenderText;
    dd->polygon = &DrawPolygon;
    dd->polyline = &DrawPolyline;
    dd->path = &DrawPath;
    dd->raster = &DrawRaster;
    dd->textUTF8 = &RenderText;
    dd->strWidthUTF8 = &GetStringWidth;


    GEaddDevice2(gd, name.c_str());
    GEinitDisplayList(gd);
    int device_number = GEdeviceNumber(gd) + 1; // to match what R says

    return Rf_ScalarInteger(device_number);

  }

  std::vector<gdi_graphics_device::GraphicsUpdateRecord> UpdatePendingGraphics(){
  //std::vector<gdi_graphics_device::Device*> UpdatePendingGraphics() {

    //std::vector<gdi_graphics_device::Device*> updates;

    for (auto device : device_list) {
      if (device->dirty()) {
        device->Repaint();
        //updates.push_back(device);
      }
    }

    return gdi_graphics_device::Device::GetUpdates();

    //return updates;
  }

}
