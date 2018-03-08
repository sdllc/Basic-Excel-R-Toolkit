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

#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <iostream>
#include <string>
#include <codecvt>

#include "gdi_graphics_device.h"
#include "variable.pb.h"
#include "controlr.h"

namespace gdi_graphics_device {

  std::vector < GraphicsUpdateRecord > Device::GetUpdates() {
    std::vector < GraphicsUpdateRecord > list;
    DWORD wait_result = WaitForSingleObject(update_lock_, 2500);
    if (wait_result == WAIT_OBJECT_0) {
      for (auto update_entry : update_list_) {
        list.push_back(update_entry.second);
      }
      update_list_.clear();
      ReleaseMutex(update_lock_);
    }
    else {
      std::cerr << "ERR aquiring update lock [2]: " << GetLastError() << std::endl;
    }
    return list;
  }

  void Device::PushUpdate(GraphicsUpdateRecord &record) {
    DWORD wait_result = WaitForSingleObject(update_lock_, 2500);
    if (wait_result == WAIT_OBJECT_0) {
      update_list_[record.name] = record;
      ReleaseMutex(update_lock_);
    }
    else {
      std::cerr << "ERR aquiring update lock [1]: " << GetLastError() << std::endl;
    }
  }

  HANDLE Device::update_lock_ = CreateMutex(0, false, 0);

  std::unordered_map <std::string, GraphicsUpdateRecord > Device::update_list_;

  __inline Gdiplus::ARGB RColor2ARGB(int color) {
    return (color & 0xff00ff00) | ((color >> 16) & 0x000000ff) | ((color << 16) & 0x00ff0000);
  }

  __inline void SetPenOptions(Gdiplus::Pen &pen, const GraphicsContext *context) {

    if (context->ljoin == ROUND_JOIN) pen.SetLineJoin(Gdiplus::LineJoin::LineJoinRound);
    else if (context->ljoin == MITRE_JOIN) pen.SetLineJoin(Gdiplus::LineJoin::LineJoinMiter);
    else if (context->ljoin == BEVEL_JOIN) pen.SetLineJoin(Gdiplus::LineJoin::LineJoinBevel);

    if (context->lend == ROUND_CAP) pen.SetLineCap(Gdiplus::LineCap::LineCapRound, Gdiplus::LineCap::LineCapRound, Gdiplus::DashCap::DashCapRound);
    else if (context->lend == BUTT_CAP) pen.SetLineCap(Gdiplus::LineCap::LineCapFlat, Gdiplus::LineCap::LineCapFlat, Gdiplus::DashCap::DashCapFlat);
    else if (context->lend == SQUARE_CAP) pen.SetLineCap(Gdiplus::LineCap::LineCapSquare, Gdiplus::LineCap::LineCapSquare, Gdiplus::DashCap::DashCapFlat);

  }

  /**
   * startup/shutdown gdi+
   */
  bool Device::InitGDIplus(bool startup) {

    static ULONG_PTR gdiplusToken = 0;

    if (!startup) {
      if (gdiplusToken) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        gdiplusToken = 0;
      }
      return true;
    }
    else {
      if (!gdiplusToken) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
      }
      return(0 != gdiplusToken);
    }

  }

  Device::Device(const std::string &name, int width, int height)
    : width_(width)
    , height_(height)
    , name_(name)
    , font_sans_(FONT_SANS_DEFAULT)
    , font_mono_(FONT_MONO_DEFAULT)
    , font_serif_(FONT_SERIF_DEFAULT)
    , bitmap_(0)
  {

    // FIXME: err
    Device::InitGDIplus(true);

    char temp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_path);

    char temp_file[MAX_PATH];
    GetTempFileNameA(temp_path, "bert_spreadsheet_device_", 0, temp_file);

    // set local
    temp_file_path_ = temp_file;

    // FIXME: temp/dev 
    //temp_file_path_ = temp_path;
    //temp_file_path_.append("\\graphics-output.png");

    Gdiplus::Bitmap *bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
    bitmap_ = (void*)bitmap;

  }

  Device::~Device() {

    Repaint(); // in case of dirty

    if (bitmap_) {
      Gdiplus::Bitmap *bitmap = (Gdiplus::Bitmap *)bitmap_;
      delete bitmap;
    }
    bitmap_ = 0;

    // the problem is that if we delete this image now, and it's repainting 
    // asynchronously, we might delete it before it's rendered.

    // actually there's a larger problem in that we are querying open devices
    // for dirtiness; that won't work if we have deleted the device.

    // ...

    // DeleteFileA(temp_file_path_.c_str()); 
  }

  void Device::MapFont(const GraphicsContext *context, std::wstring &font_name, int32_t &style, double &font_size) {

    std::string name(context->fontfamily);

    if (name == "sans" || name == "sans-serif" || name == "") font_name = font_sans_;
    else if (name == "serif") font_name = font_serif_;
    else if (name == "mono" || name == "monospace") font_name = font_mono_;
    else if (context->fontface == 5) {
      font_name = L"symbol";
    }
    else {
      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter_;
      font_name = converter_.from_bytes(name);
    }

    style = 0;

    if ((context->fontface == 2) || (context->fontface == 4)) style |= Gdiplus::FontStyle::FontStyleBold;
    if ((context->fontface == 3) || (context->fontface == 4)) style |= Gdiplus::FontStyle::FontStyleItalic;

    font_size = context->cex * context->ps / 96.0 * 128.0; // points -> pixels

  }

  /**
   * see
   * https://msdn.microsoft.com/en-us/library/windows/desktop/ms533843(v=vs.85).aspx
   */
  void * Device::GetPngCLSID() /// const WCHAR* format, CLSID* pClsid)
  {
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    static CLSID *png_clsid = 0;
    if (png_clsid) return (void*)png_clsid;

    Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
      return 0;  // Failure

    pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
      return 0;  // Failure

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
      if (wcscmp(pImageCodecInfo[j].MimeType, L"image/png") == 0)
      {
        png_clsid = new CLSID(pImageCodecInfo[j].Clsid);
        break;
      }
    }

    free(pImageCodecInfo);
    if (png_clsid) return (void*)png_clsid;
    return 0;

  }

  void Device::Update() {
    dirty_ = true;
  }

  void Device::Repaint() {

    if (!dirty_) return;
    dirty_ = false;

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter_;
    std::wstring wide = converter_.from_bytes(temp_file_path_);

    CLSID *png_clsid = static_cast<CLSID*>(Device::GetPngCLSID());
    if (png_clsid) {
      ((Gdiplus::Bitmap*)bitmap_)->Save(wide.c_str(), png_clsid, 0);
    }

    PushUpdate(GraphicsUpdateRecord({ name_, temp_file_path_, width_, height_ }));

  }

  void Device::UpdateSize() {

    BERTBuffers::CallResponse call, response;
    call.set_wait(true);
    auto function_call = call.mutable_function_call();
    function_call->set_target(BERTBuffers::CallTarget::graphics);
    auto graphics_update = function_call->add_arguments()->mutable_graphics();
    graphics_update->set_command(BERTBuffers::GraphicsUpdateCommand::query_size);
    graphics_update->set_name(name());
    Callback(call, response);

    auto result = response.result();

    if (result.value_case() == BERTBuffers::Variable::ValueCase::kGraphics) {
      const auto &graphics_response = result.graphics();

      uint32_t width = graphics_response.width();
      uint32_t height = graphics_response.height();

      if (width_ != width || height_ != height) {
        Gdiplus::Bitmap *bitmap;
        if (bitmap_) {
          bitmap = (Gdiplus::Bitmap *)bitmap_;
          delete bitmap;
        }
        bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
        bitmap_ = (void*)bitmap;
        width_ = width;
        height_ = height;
      }

    }

  }

  void Device::NewPage(const GraphicsContext *context, int32_t width, int32_t height, uint32_t color) {

    // this won't happen here, it will be called before this in UpdateSize

    if (width_ != width || height_ != height) {
      Gdiplus::Bitmap *bitmap;
        if (bitmap_) {
          bitmap = (Gdiplus::Bitmap *)bitmap_;
          delete bitmap;
        }
      bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
      bitmap_ = (void*)bitmap;
      width_ = width;
      height_ = height;
    }

    Gdiplus::Graphics graphics((Gdiplus::Bitmap*)bitmap_);
    Gdiplus::SolidBrush bg(Gdiplus::Color(RColor2ARGB(color)));
    graphics.FillRectangle(&bg, 0, 0, width, height);

    Update();

  }

  void Device::DrawLine(const GraphicsContext *context, double x1, double y1, double x2, double y2) {

    Gdiplus::Graphics graphics((Gdiplus::Bitmap*)bitmap_);
    graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

    Gdiplus::Pen stroke(RColor2ARGB(context->col), context->lwd);
    SetPenOptions(stroke, context);

    graphics.DrawLine(&stroke, (Gdiplus::REAL)x1, (Gdiplus::REAL)y1, (Gdiplus::REAL)x2, (Gdiplus::REAL)y2);
    Update();

  }

  void Device::DrawPoly(const GraphicsContext *context, int32_t n, double *x, double *y, int32_t filled) {

    Gdiplus::Graphics graphics((Gdiplus::Bitmap*)bitmap_);
    graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
    Gdiplus::Pen stroke(RColor2ARGB(context->col), context->lwd);
    SetPenOptions(stroke, context);

    if (n < 1) return;

    Gdiplus::PointF *pt = new Gdiplus::PointF[n];

    for (int i = 0; i < n; i++) {
      pt[i].X = x[i];
      pt[i].Y = y[i];
    }

    graphics.DrawLines(&stroke, pt, n);

    if (filled) {
      Gdiplus::SolidBrush fill(RColor2ARGB(context->fill));
      graphics.FillPolygon(&fill, pt, n);
    }

    delete[] pt;
    Update();

  }

  void Device::DrawRect(const GraphicsContext *context, double x1, double y1, double x2, double y2) {

    Gdiplus::Graphics graphics((Gdiplus::Bitmap*)bitmap_);
    graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

    Gdiplus::Pen stroke(RColor2ARGB(context->col), context->lwd);
    Gdiplus::SolidBrush fill(RColor2ARGB(context->fill));

    SetPenOptions(stroke, context);

    Gdiplus::REAL x = x1;
    Gdiplus::REAL y = y1;

    Gdiplus::REAL w = x2 - x1;
    Gdiplus::REAL h = y2 - y1;

    if (w < 0) { x = x2; w = -w; }
    if (h < 0) { y = y2; h = -h; }

    graphics.FillRectangle(&fill, x, y, w, h);
    graphics.DrawRectangle(&stroke, x, y, w, h);

    Update();

  }

  void Device::DrawCircle(const GraphicsContext *context, double x, double y, double r) {

    Gdiplus::Graphics graphics((Gdiplus::Bitmap*)bitmap_);
    graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

    Gdiplus::Pen stroke(RColor2ARGB(context->col), context->lwd);
    SetPenOptions(stroke, context);

    Gdiplus::SolidBrush fill(RColor2ARGB(context->fill));

    Gdiplus::RectF rect(x - r, y - r, r * 2, r * 2);

    graphics.FillEllipse(&fill, rect);
    graphics.DrawEllipse(&stroke, rect);

    Update();
  }

  void Device::RenderText(const GraphicsContext *context, const char *text, double x, double y, double rot) {

    Gdiplus::Graphics graphics((Gdiplus::Bitmap*)bitmap_);

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter_;
    std::wstring wide_string = converter_.from_bytes(text);

    std::wstring font_name;
    int32_t style = 0;
    double font_size;

    MapFont(context, font_name, style, font_size);
    Gdiplus::Font font(font_name.c_str(), font_size, style, Gdiplus::Unit::UnitPixel);
    Gdiplus::FontFamily family;

    font.GetFamily(&family);

    //Gdiplus::StringFormat format;
    //format.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);

    Gdiplus::PointF origin(x, y);
    Gdiplus::SolidBrush fill(RColor2ARGB(context->col)); // R says stroke, GDI+ says fill

    UINT16 ascent_design_units = family.GetCellAscent(font.GetStyle());
    UINT16 ascent_pixels = (UINT16)(font.GetSize() * ascent_design_units / family.GetEmHeight(font.GetStyle()));

    Gdiplus::RectF bounding_rect;
    graphics.MeasureString(wide_string.c_str(), wide_string.length(), &font, origin, &bounding_rect);

    //    Gdiplus::Pen redpen(Gdiplus::Color::Red);
    //    graphics.DrawRectangle(&redpen, bounding_rect);

        // UPDATE: rounding to pixel.  that might only be necessary in the 
        // vertical plane (not sure).  however assuming the same font, while this
        // might offset it will offset consistently.

        // rot is in degrees, same as gdi+

    if (rot) {

      origin.X = roundf(origin.X);
      origin.Y = roundf(origin.Y);

      graphics.TranslateTransform(origin.X, origin.Y);
      graphics.RotateTransform(-rot);
      origin.X = 0;
      origin.Y = -ascent_pixels; // baseline

      // "TextRenderingHintAntiAlias provides the best quality for rotated text."
      graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    }
    else {

      // baseline
      origin.Y -= ascent_pixels;
      origin.X = roundf(origin.X);
      origin.Y = roundf(origin.Y);

    }

    graphics.DrawString(wide_string.c_str(), wide_string.length(), &font, origin, &fill);

    if (rot) graphics.ResetTransform();

    Update();

  }

  void Device::FontMetrics(const GraphicsContext *context, const std::string &text, double *ascent, double *descent, double *width) {

    static int canvas_width = 200;
    static int canvas_height = 200;

    static Gdiplus::Bitmap bitmap(canvas_width, canvas_height, PixelFormat32bppARGB);
    static Gdiplus::SolidBrush black_brush(Gdiplus::Color::Black);
    static Gdiplus::SolidBrush white_brush(Gdiplus::Color::White);

    double font_size;
    int32_t style;
    std::wstring font_name;

    MapFont(context, font_name, style, font_size);
    Gdiplus::Font font(font_name.c_str(), font_size, style, Gdiplus::Unit::UnitPixel);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter_;

    Gdiplus::Graphics graphics(&bitmap);
    Gdiplus::PointF center(8, canvas_height / 2);
    Gdiplus::RectF bounding_rect;
    Gdiplus::Color clr;
    Gdiplus::FontFamily family;

    std::wstring wide_string = converter_.from_bytes(text);

    graphics.MeasureString(wide_string.c_str(), wide_string.length(), &font, center, &bounding_rect);

    // std::cout << "Font size " << font_size << ", get size " << font.GetSize() << std::endl;

    // https://groups.google.com/forum/#!topic/microsoft.public.platformsdk.gdi/g_EZpq1o-NI

    font.GetFamily(&family);
    UINT16 ascent_design_units = family.GetCellAscent(font.GetStyle());
    UINT16 ascent_pixels = (UINT16)(font.GetSize() * ascent_design_units / family.GetEmHeight(font.GetStyle()));

    Gdiplus::PointF start(center.X, center.Y - ascent_pixels);
    graphics.FillRectangle(&black_brush, (int)(start.X - 4), (int)(start.Y - 4), (int)(start.X + bounding_rect.Width + 4), (int)(start.Y + bounding_rect.Height + 4));

    graphics.DrawString(wide_string.c_str(), wide_string.length(), &font, start, &white_brush);

    int first_pixel = canvas_height;
    int last_pixel = 0;

    int first_x_pixel = canvas_width;
    int x_pixel = 0;

    for (int x = start.X - 1; x < start.X + bounding_rect.Width + 2; x++) {
      for (int y = start.Y - 1; y < start.Y + bounding_rect.Height + 2; y++) {
        bitmap.GetPixel(x, y, &clr);
        if (clr.GetValue() & 0x00ffffff) {
          if (y < first_pixel) first_pixel = y;
          if (y > last_pixel) last_pixel = y;
          x_pixel = x;
          if (x < first_x_pixel) first_x_pixel = x;
        }
      }
    }

    *width = x_pixel - first_x_pixel + 4;

    *ascent = ((canvas_height / 2) - first_pixel);
    *descent = (last_pixel - (canvas_height / 2));

    if (*ascent < 0) *ascent = 0;
    if (*descent < 0) *descent = 0;

    //printf("Compare: fixed %01.02f, original %01.02f\n", *width, bounding_rect.Width);
    //printf("Test %s: ascent %01.02f, descent %01.02f (%d, %d)\n", text.c_str(), *ascent, *descent, first_pixel, last_pixel);

  }

  void Device::MeasureText(const GraphicsContext *context, const char *text, double *width, double *height) {

    Gdiplus::Graphics graphics((Gdiplus::Bitmap*)bitmap_);
    graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter_;
    std::wstring wide_string = converter_.from_bytes(text);

    Gdiplus::PointF origin(0, 0);
    Gdiplus::RectF bounding_rect;

    std::wstring font_name;
    int32_t style = 0;
    double font_size = 0;

    MapFont(context, font_name, style, font_size);
    Gdiplus::Font font(font_name.c_str(), font_size, style, Gdiplus::Unit::UnitPixel);

    graphics.MeasureString(wide_string.c_str(), wide_string.length(), &font, origin, &bounding_rect);

    if (width) *width = bounding_rect.Width;
    if (height) *height = bounding_rect.Height;

  }

  void Device::DrawBitmap(unsigned int* data, int pixel_width, int pixel_height, double x, double y, double target_width, double target_height, double rot){

    Gdiplus::Graphics graphics((Gdiplus::Bitmap*)bitmap_);
    graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationMode::InterpolationModeHighQuality);

    // R data comes in 32-bit RGBA (lsb first byte-order).  There's no matching GDI+ format, 
    // so we have to munge the data.  when GDI+ says "ARGB" that means word order, or BGRA 
    // in lsb-first byte-order. we have to swap R and B.

    int len = pixel_width * pixel_height;
    unsigned int *pixels = new unsigned int[len];

    for (int i = 0; i < len; i++) {
      pixels[i] = (data[i] & 0xff00ff00) | ((data[i] >> 16) & 0x000000ff) | ((data[i] << 16) & 0x00ff0000);
    }

    Gdiplus::Bitmap bmp(pixel_width, pixel_height, 4 * pixel_width, PixelFormat32bppARGB, (unsigned char*)pixels);
    
    if (target_height < 0) {
      y += target_height;
      target_height = -target_height;
    }

    if (target_width < 0) {
      x += target_width;
      target_width = -target_width;
    }

    // gdiplus scaling is crummy because it samples in positive directions only. at the edge 
    // of the sample image, instead of sampling in the other direction, it just uses transparent
    // pixels, so you get fading at the edge (this is true of all interpolation modes). the way 
    // to fix this would be to write a proper scaling method, but short of that a simple workaround
    // is to tile the image and then scale from the original. depending on kernel size this could
    // be simplified to paint only some smaller number of pixels to the target. 

    // so this is hacky, and wasteful, but a huge improvement.

    Gdiplus::Bitmap tiled(pixel_width * 2, pixel_height * 2);
    Gdiplus::Graphics tiled_graphics(&tiled);
    tiled_graphics.DrawImage(&bmp, 0, 0);

    bmp.RotateFlip(Gdiplus::RotateNoneFlipX);
    tiled_graphics.DrawImage(&bmp, pixel_width, 0);

    bmp.RotateFlip(Gdiplus::RotateNoneFlipY);
    tiled_graphics.DrawImage(&bmp, pixel_width, pixel_height);

    bmp.RotateFlip(Gdiplus::RotateNoneFlipX);
    tiled_graphics.DrawImage(&bmp, 0, pixel_height);

    Gdiplus::RectF source_rect(0, 0, pixel_width, pixel_height);
    Gdiplus::RectF target_rect(x, y, target_width, target_height);

    // NOTE: the rect -> rect method is only available in a higher 
    // level of gdiplus than we want to support

    graphics.DrawImage(
      &tiled,       // IN Image* image,
      target_rect,  // IN const Rect& destRect,
      0,            // IN INT srcx,
      0,            // IN INT srcy,
      pixel_width,  // IN INT srcwidth,
      pixel_height, // IN INT srcheight,
      Gdiplus::Unit::UnitPixel // IN Unit srcUnit,
    );

    delete[] pixels;

    Update();

  }

};

