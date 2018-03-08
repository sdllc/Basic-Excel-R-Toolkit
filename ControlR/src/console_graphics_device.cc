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

namespace ConsoleGraphicsDevice {

  __inline void RColorToVariableColor(BERTBuffers::Color *target, uint32_t src) {
    target->set_a((src >> 24) & 0xff);
    target->set_b((src >> 16) & 0xff);
    target->set_g((src >> 8) & 0xff);
    target->set_r((src >> 0) & 0xff);
  }

  void SetMessageContext(BERTBuffers::GraphicsContext *target, const pGEcontext gc) {

    auto color = target->mutable_col();
    auto fill = target->mutable_fill();

    RColorToVariableColor(color, gc->col);
    RColorToVariableColor(fill, gc->fill);

    target->set_gamma(gc->gamma);
    target->set_lwd(gc->lwd);
    target->set_lty(gc->lty);
    target->set_lend(gc->lend);
    target->set_ljoin(gc->ljoin);
    target->set_lmitre(gc->lmitre);
    target->set_cex(gc->cex);
    target->set_ps(gc->ps);
    target->set_lineheight(gc->lineheight);
    target->set_fontface(gc->fontface); // can unpack into flags
    target->set_fontfamily(gc->fontfamily);
  }

  void SetClip(double x0, double x1, double y0, double y1, pDevDesc dd) {
    //std::cout << "g: set clip" << std::endl;

    BERTBuffers::CallResponse message;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());

    // SetMessageContext(graphics->mutable_context(), gc);

    graphics->set_command("set-clip");
    graphics->add_x(x0);
    graphics->add_y(y0);
    graphics->add_x(x1);
    graphics->add_y(y1);

    PushConsoleMessage(message);

  }

  void NewPage(const pGEcontext gc, pDevDesc dd) {
    // std::cout << "g: new page: " << dd->right << ", " << dd->bottom << std::endl;
    BERTBuffers::CallResponse message;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);

    graphics->set_command("new-page");
    graphics->add_x(dd->right);
    graphics->add_y(dd->bottom);

    PushConsoleMessage(message);
  }

  void DrawLine(double x1, double y1, double x2, double y2, const pGEcontext gc, pDevDesc dd) {
    // std::cout << "g: draw line" << std::endl;
    BERTBuffers::CallResponse message;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);

    graphics->set_command("draw-line");
    graphics->add_x(x1);
    graphics->add_x(x2);
    graphics->add_y(y1);
    graphics->add_y(y2);
    PushConsoleMessage(message);
  }

  void DrawPoly(int n, double *x, double *y, bool filled, const pGEcontext gc, pDevDesc dd) {

    BERTBuffers::CallResponse message;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);

    graphics->set_command("draw-polyline");
    graphics->set_filled(filled);
    for (int i = 0; i < n; i++) {
      graphics->add_x(x[i]);
      graphics->add_y(y[i]);
    }
    PushConsoleMessage(message);

  }

  void DrawPolyline(int n, double *x, double *y, const pGEcontext gc, pDevDesc dd) {
    // std::cout << "g: draw polyline" << std::endl;
    DrawPoly(n, x, y, false, gc, dd);
  }

  void DrawPolygon(int n, double *x, double *y, const pGEcontext gc, pDevDesc dd) {
    //std::cout << "g: draw polygon" << std::endl;
    DrawPoly(n, x, y, true, gc, dd);
  }

  void DrawPath(double *x, double *y,
    int npoly, int *nper,
    Rboolean winding,
    const pGEcontext gc, pDevDesc dd) {
    std::cerr << "ENOTIMPL: draw_path" << std::endl; // FIXME
  }

  double GetStringWidth(const char *str, const pGEcontext gc, pDevDesc dd) {

    BERTBuffers::CallResponse message, response;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);
    graphics->set_command("measure-text");
    graphics->set_text(str);

    bool success = ConsoleCallback(message, response);
    if (success) {
      if (response.operation_case() == BERTBuffers::CallResponse::OperationCase::kConsole) {
        auto graphics = response.console().graphics();
        if (graphics.x_size()) {
          return graphics.x(0);
        }
      }
    }

    return 10;
  }

  void DrawRect(double x1, double y1, double x2, double y2,
    const pGEcontext gc, pDevDesc dd) {
    //std::cout << "g: draw rect" << std::endl;
    BERTBuffers::CallResponse message;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);

    graphics->set_command("draw-rect");
    graphics->add_x(x1);
    graphics->add_x(x2);
    graphics->add_y(y1);
    graphics->add_y(y2);
    PushConsoleMessage(message);
  }

  void DrawCircle(double x, double y, double r, const pGEcontext gc,
    pDevDesc dd) {
    //std::cout << "g: draw circle" << std::endl;
    BERTBuffers::CallResponse message;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);

    graphics->set_command("draw-circle");
    graphics->add_x(x);
    graphics->add_y(y);
    graphics->set_r(r);
    PushConsoleMessage(message);
  }

  void RenderText(double x, double y, const char *str, double rot,
    double hadj, const pGEcontext gc, pDevDesc dd) {
    //std::cout << "g: draw text" << std::endl;
    BERTBuffers::CallResponse message;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);

    graphics->set_command("draw-text");
    graphics->add_x(x);
    graphics->add_y(y);
    graphics->set_rot(rot);
    graphics->set_hadj(hadj);
    graphics->set_text(str);
    PushConsoleMessage(message);
  }

  void GetSize(double *left, double *right, double *bottom, double *top, pDevDesc dd) {
    // std::cout << "g: get size (" << dd->left << ", " << dd->top << ", " << dd->right << ", " << dd->bottom << ")" << std::endl;

    *left = dd->left;
    *right = dd->right;
    *bottom = dd->bottom;
    *top = dd->top;
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

    BERTBuffers::CallResponse message, response;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);
    graphics->set_command("font-metrics");
    graphics->set_text(str);

    bool success = ConsoleCallback(message, response);
    if (success) {
      if (response.operation_case() == BERTBuffers::CallResponse::OperationCase::kConsole) {
        auto graphics = response.console().graphics();
        if (graphics.x_size() && graphics.y_size() > 1) {

          *width = graphics.x(0);
          *ascent = graphics.y(0);
          *descent = graphics.y(1);

          return;
        }
      }
    }

    *width = 10;
    *ascent = 10.5;
    *descent = 0;
  }

  void DrawRaster(unsigned int *raster,
    int pixel_width, int pixel_height,
    double x, double y,
    double target_width, double target_height,
    double rot,
    Rboolean interpolate,
    const pGEcontext gc, pDevDesc dd) {

    // std::cout << "draw raster" << std::endl;
    BERTBuffers::CallResponse message;
    auto graphics = message.mutable_console()->mutable_graphics();
    graphics->set_device_type(((std::string*)(dd->deviceSpecific))->c_str());
    SetMessageContext(graphics->mutable_context(), gc);

    graphics->set_command("draw-raster");

    // this is contract

    graphics->add_x(x);
    graphics->add_y(y);

    graphics->add_x(pixel_width);
    graphics->add_y(pixel_height);

    graphics->add_x(target_width);
    graphics->add_y(target_height);

    graphics->set_rot(rot);
    graphics->set_interpolate(interpolate);

    // if we want to do any bit/byte/format conversion, we should do 
    // it here rather than on the client, we should be more efficient.

    graphics->set_raster(raster, sizeof(unsigned int) * pixel_width * pixel_height); // int?

    PushConsoleMessage(message);
  }

  void CloseDevice(pDevDesc dd) {
    if (dd->deviceSpecific) delete (dd->deviceSpecific);
    dd->deviceSpecific = 0;
  }

  SEXP CreateConsoleDevice(const std::string &background, double width, double height, double pointsize, const std::string &type, void * pointer) {

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

    // now functions, at least the ones we're implementing

    dd->newPage = &NewPage;
    dd->close = &CloseDevice;
    dd->clip = &SetClip;
    dd->size = &GetSize;
    dd->metricInfo = &GetMetricInfo;
    dd->strWidth = &GetStringWidth;
    dd->line = &DrawLine;
    dd->text = &RenderText;
    dd->rect = &DrawRect;
    dd->circle = &DrawCircle;
    dd->polygon = &DrawPolygon;
    dd->polyline = &DrawPolyline;
    dd->path = &DrawPath;
    dd->raster = &DrawRaster;
    dd->textUTF8 = &RenderText;
    dd->strWidthUTF8 = &GetStringWidth;

    // force svg or png

    std::string *normalized_type = new std::string("svg");
    if (!type.compare("png")) (*normalized_type) = "png";

    std::cout << "init device (" << (*normalized_type) << "): " << std::dec << dd->right << ", " << dd->bottom << std::endl;
    dd->deviceSpecific = normalized_type;

    std::string name = "BERT Console (";
    name.append(normalized_type->c_str());
    name.append(")");

    GEaddDevice2(gd, name.c_str());
    GEinitDisplayList(gd);
    int device = GEdeviceNumber(gd) + 1; // to match what R says

    return Rf_ScalarInteger(device);

  }

};
