
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

  void close_device(pDevDesc dd) {

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
  
  void new_page(const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->NewPage(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), dd->right, dd->bottom, dd->startfill );
  }

  void draw_line(double x1, double y1, double x2, double y2, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawLine(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), x1, y1, x2, y2);
  }

  void draw_rect(double x1, double y1, double x2, double y2, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawRect(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), x1, y1, x2, y2);
  }

  void draw_circle(double x, double y, double r, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawCircle(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), x, y, r);
  }
  
  void draw_poly(int n, double *x, double *y, bool filled, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    device->DrawPoly(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), n, x, y, filled);
  }

  void draw_polyline(int n, double *x, double *y, const pGEcontext gc, pDevDesc dd) {
    draw_poly(n, x, y, false, gc, dd);
  }

  void draw_polygon(int n, double *x, double *y, const pGEcontext gc, pDevDesc dd) {
    draw_poly(n, x, y, true, gc, dd);
  }

  void get_size(double *left, double *right, double *bottom, double *top, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    *left = dd->left;
    *top = dd->top;
    *right = (dd->left + device->width());
    *bottom = (dd->top + device->height());
  }

  double get_strWidth(const char *str, const pGEcontext gc, pDevDesc dd) {
    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    double width, height;
    device->MeasureText(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), str, &width, &height);
    return width;
  }

  void draw_text(double x, double y, const char *str, double rot, double hadj, const pGEcontext gc, pDevDesc dd) {

    gdi_graphics_device::Device *device = (gdi_graphics_device::Device*)(dd->deviceSpecific);
    if (hadj) {
      std::cerr << "WARNING: hadj " << hadj << std::endl;
    }
    device->RenderText(reinterpret_cast<gdi_graphics_device::GraphicsContext*>(gc), str, x, y, rot);
    
  }
  
  void get_metric_info(int c, const pGEcontext gc, double* ascent, double* descent, double* width, pDevDesc dd) {

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

  void draw_raster(unsigned int *raster,
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

  void draw_path(double *x, double *y, int npoly, int *nper, Rboolean winding, const pGEcontext gc, pDevDesc dd) {
    std::cerr << "ENOTIMPL: draw_path" << std::endl; // FIXME
  }

  void set_clip(double x0, double x1, double y0, double y1, pDevDesc dd) {}

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

    std::cout << "init device (" << name << "): " << std::dec << dd->right << ", " << dd->bottom << std::endl;
    gdi_graphics_device::Device *device = new gdi_graphics_device::Device(name, width, height);
    device_list.push_back(device);

    dd->deviceSpecific = device;
    dd->newPage = &new_page;
    dd->close = &close_device;
    dd->line = &draw_line;
    dd->rect = &draw_rect;
    dd->circle = &draw_circle;

    dd->clip = &set_clip;
    dd->size = &get_size;
    dd->metricInfo = &get_metric_info;
    dd->strWidth = &get_strWidth;
    dd->text = &draw_text;
    dd->polygon = &draw_polygon;
    dd->polyline = &draw_polyline;
    dd->path = &draw_path;
    dd->raster = &draw_raster;
    dd->textUTF8 = &draw_text;
    dd->strWidthUTF8 = &get_strWidth;


    GEaddDevice2(gd, name.c_str());
    GEinitDisplayList(gd);
    int device_number = GEdeviceNumber(gd) + 1; // to match what R says

    return Rf_ScalarInteger(device_number);

  }

  void UpdatePendingGraphics() {
    for (auto device : device_list) {
      if (device->dirty()) device->Repaint();
    }
  }

}
