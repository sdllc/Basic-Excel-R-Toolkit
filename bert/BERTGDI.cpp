/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014-2017 Structured Data, LLC
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
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*/

#include "stdafx.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "DebugOut.h"
#include "BERTGDI.h"

#include <vector>

// mapped from R graphics device

#define GE_ROUND_CAP	1
#define GE_BUTT_CAP		2
#define GE_SQUARE_CAP	3

#define GE_ROUND_JOIN	1
#define GE_MITRE_JOIN	2
#define GE_BEVEL_JOIN	3

extern HWND getExcelHWND();
extern LPDISPATCH pdispApp;

extern void bert_device_resize_(int dev, double width, double height, bool replay);
extern void bert_device_init(void *name, void *p);
extern void bert_device_shutdown(void *p);

std::vector < BERTGraphicsDevice * > dlist;

// we need this for timer messages.  can we keep it, though? is it persistent?
HWND hwndExcel = 0;

//-------------------------------------------------------------------------
//
// excel API support
//

// the proper way to do this is via import.  however that's a bit fragile
// as it requires a consistent path to DLLs that we can't distribute.  we 
// should be able to use cached versions of these files, though, they're 
// not going to change.

#include "mso-15.tlh"
#include "excel-15.tlh"

//-------------------------------------------------------------------------
// 
// ATL
// 

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>

/**
 * startup/shutdown gdi+
 */
bool initGDIplus(bool startup) {

	if (!hwndExcel) hwndExcel = getExcelHWND();

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

//
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms533843(v=vs.85).aspx
//
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

void createDeviceTarget( const WCHAR *name, CComPtr< Excel::Shape > &target, double w, double h ) {

	HRESULT hr;

	std::basic_string<WCHAR> compound_name = L"BGD_";
	compound_name += name;

	DebugOut("creating device target\n");

	HDC hdcScreen = ::GetDC(NULL);
	int logpixels = ::GetDeviceCaps(hdcScreen, LOGPIXELSX);
	::ReleaseDC(NULL, hdcScreen);

	std::basic_string< WCHAR > alttext = L"BERT/R Graphics Device Target: ";
	alttext += name;

	if (pdispApp) {
		CComQIPtr< Excel::_Application > app(pdispApp);
		if (app) {
			CComPtr<IDispatch> pdispsheet;
			if (SUCCEEDED(app->get_ActiveSheet(&pdispsheet))) {
				CComQIPtr< Excel::_Worksheet > sheet(pdispsheet);
				if (sheet) {
					CComPtr< Excel::Shapes > shapes;
					sheet->get_Shapes(&shapes);
					if (shapes) {
						Excel::IShapes *ishapes = (Excel::IShapes*)(shapes.p);
						if (ishapes) {
							CComPtr< Excel::Shape > shape;
							ishapes->AddShape(Office::msoShapeRectangle, 100, 100, (float)(w * 72 / logpixels), (float)(h * 72 / logpixels), &shape);
							Excel::IShape *ishape = (Excel::IShape*)(shape.p);
							if (ishape) {
								int rc = ishape->AddRef();
								CComBSTR bstr = compound_name.c_str();
								ishape->put_Name(bstr);
								bstr = alttext.c_str();
								ishape->put_AlternativeText(bstr);
								CComPtr< Excel::LineFormat > line;
								ishape->get_Line(&line);
								if (line) line->put_Visible(Office::MsoTriState::msoFalse);
								ishape->Release();
							}
							target = shape.p;
						}
					}
				}
			}
		}
	}

}

void findDeviceTarget(const WCHAR *name, CComPtr<Excel::Shape> &target ) {

	HRESULT hr = S_OK;

	std::basic_string<WCHAR> compound_name = L"BGD_";
	compound_name += name;
	
	if (pdispApp) {

		CComPtr<Excel::Workbooks> workbooks;
		CComQIPtr<Excel::_Application> app(pdispApp);

		if (app) app->get_Workbooks(&workbooks);
		if (workbooks) {

			long wbcount = 0;
			workbooks->get_Count(&wbcount);
			for (long lwb = 0; lwb < wbcount; lwb++) {

				CComVariant vwb = lwb + 1;
				CComPtr<Excel::_Workbook> book;
				HRESULT hr = workbooks->get_Item(vwb, &book);

				if (SUCCEEDED(hr)) {

					CComPtr<Excel::Sheets> sheets;
					if (book) book->get_Worksheets(&sheets);

					if (sheets) {

						long count = 0;
						sheets->get_Count(&count);

						for (long l = 1; l <= count; l++) {
							CComVariant var = l;
							CComPtr<IDispatch> pdispsheet;

							if (SUCCEEDED(sheets->get_Item(var, &pdispsheet))) {
								CComQIPtr< Excel::_Worksheet > sheet(pdispsheet);
								if (sheet) {
									CComPtr<Excel::Shapes> shapes;
									sheet->get_Shapes(&shapes);
									if (shapes) {
										Excel::IShapes* ishapes = (Excel::IShapes*)(shapes.p);
										ishapes->AddRef();
										CComVariant cvItem = compound_name.c_str();
										Excel::Shape *pshape = 0;
										if (SUCCEEDED(ishapes->Item(cvItem, &pshape))) {
											pshape->AddRef();
											target = pshape;
											pshape->Release();
											return;
										}
										ishapes->Release();
									}
								}
							}
						}

					}
				}
			}
		}
	}

}

void timer_callback(HWND hwnd, UINT msg, UINT_PTR id, DWORD dwtime) {
	::KillTimer(hwndExcel, id);
	for (std::vector< BERTGraphicsDevice*>::iterator iter = dlist.begin(); iter != dlist.end(); iter++) {
		BERTGraphicsDevice *device = *iter;
		if (device->dirty) device->repaint();
	}
}

/**
 * when graphics update, we don't necessarily want to paint immediately.  our paint
 * is very expensive, and R graphics are drawn with lots of individual updates.
 * so we toll for (X) ms and only repaint when things settle.
 *
 * FIXME/TODO: maybe have absolute paints every (Y) ms to show progress on very complex graphics?
 */
void BERTGraphicsDevice::update() {
	dirty = true;
	UINT_PTR id = WM_USER + 100;
	::SetTimer(hwndExcel, id, 100, (TIMERPROC)timer_callback);
}

/**
 * this is the actual paint op, which happens on timer events.
 */
void BERTGraphicsDevice::repaint() {

	if (!dirty) return;

	dirty = false;
	DebugOut(" ** graphics update\n");

//	std::basic_string<WCHAR> compound_name = L"BGD_";
//	compound_name += name;

	CLSID pngClsid;
	GetEncoderClsid(L"image/png", &pngClsid);
	((Gdiplus::Bitmap*)pbitmap)->Save(tempFile, &pngClsid, NULL);
	
	CComPtr< Excel::Shape > shape;
	findDeviceTarget(name, shape);
	if (!shape) createDeviceTarget(name, shape, width, height);

	if (shape) {
		CComPtr< Excel::FillFormat > fill;
		Excel::IShape *ishape = (Excel::IShape*)(shape.p);
		if (ishape) ishape->get_Fill(&fill);
		if (fill) fill->UserPicture(tempFile);
	}

}

inline Gdiplus::ARGB RColor2ARGB(int color) {
	return (color & 0xff00ff00) | ((color >> 16) & 0x000000ff) | ((color << 16) & 0x00ff0000);
}

void setPenOptions(Gdiplus::Pen &pen, GraphicsStyle *gs) {

	if ( gs->ljoin == GE_ROUND_JOIN ) pen.SetLineJoin(Gdiplus::LineJoin::LineJoinRound);
	else if ( gs->ljoin == GE_MITRE_JOIN ) pen.SetLineJoin(Gdiplus::LineJoin::LineJoinMiter);
	else if ( gs->ljoin == GE_BEVEL_JOIN ) pen.SetLineJoin(Gdiplus::LineJoin::LineJoinBevel);

	if (gs->lend == GE_ROUND_CAP) pen.SetLineCap(Gdiplus::LineCap::LineCapRound, Gdiplus::LineCap::LineCapRound, Gdiplus::DashCap::DashCapRound);
	else if (gs->lend == GE_BUTT_CAP ) pen.SetLineCap(Gdiplus::LineCap::LineCapFlat, Gdiplus::LineCap::LineCapFlat, Gdiplus::DashCap::DashCapFlat);
	else if (gs->lend == GE_SQUARE_CAP ) pen.SetLineCap(Gdiplus::LineCap::LineCapSquare, Gdiplus::LineCap::LineCapSquare, Gdiplus::DashCap::DashCapFlat);

}

std::basic_string < WCHAR > BERTGraphicsDevice::mapFontName(std::string name) {

	if (name == "sans" || name == "sans-serif" || name == "") return font_sans;
	else if (name == "serif") return font_serif;
	else if (name == "mono" || name == "monospace") return font_mono;

	// otherwise map directly

	int len = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name.length(), 0, 0);
	if (len <= 0) return font_sans;

	WCHAR *wsz = new WCHAR[len + 1];
	int err = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name.length(), wsz, len);
	wsz[len] = 0;

	std::basic_string<WCHAR> result = wsz;

	delete[] wsz;

	return result;
}

void BERTGraphicsDevice::drawText(const char *str, double x, double y, double rot, GraphicsStyle *gs) {
	
	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);

	int len = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), 0, 0);
	if (len <= 0) return;

	WCHAR *wsz = new WCHAR[len + 1];
	int err = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), wsz, len);
	wsz[len] = 0;

	if (!err) {
		DWORD dwerr = GetLastError();
		DebugOut("MB ERR %d\n", dwerr);
	}

	std::basic_string<WCHAR> fontname = mapFontName(gs->fontname);
	int style = 0; // Gdiplus::FontStyle::FontStyleRegular;
	if (gs->bold) style |= Gdiplus::FontStyle::FontStyleBold;
	if (gs->italic) style |= Gdiplus::FontStyle::FontStyleItalic;

	Gdiplus::Font font(fontname.c_str(), gs->fontsize, style, Gdiplus::Unit::UnitPixel);

	Gdiplus::PointF origin(x, y);
	Gdiplus::SolidBrush fill(RColor2ARGB(gs->col)); // R says stroke, GDI+ says fill

	Gdiplus::RectF boundRect;
	graphics.MeasureString(wsz, len, &font, origin, &boundRect);

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
		origin.Y = round( -boundRect.Height );

		// "TextRenderingHintAntiAlias provides the best quality for rotated text."
		graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
	}
	else {
		origin.X = roundf(origin.X);
		origin.Y = roundf(origin.Y - boundRect.Height);
	}

	graphics.DrawString(wsz, len, &font, origin, &fill);

	if (rot) graphics.ResetTransform();

	delete[] wsz;

}

void BERTGraphicsDevice::measureText(const char *str, GraphicsStyle *gs, double *width, double *height) {

	*width = *height = 0;
	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

	int len = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), 0, 0);
	if (len <= 0) return;

	WCHAR *wsz = new WCHAR[len + 1];
	int err = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), wsz, len);
	wsz[len] = 0;

	if (!err) {
		DWORD dwerr = GetLastError();
		DebugOut("MB ERR %d\n", dwerr);
	}

	Gdiplus::PointF origin(0, 0);
	Gdiplus::RectF boundRect;

	std::basic_string<WCHAR> fontname = mapFontName(gs->fontname);

	int style = 0; // Gdiplus::FontStyle::FontStyleRegular;
	if (gs->bold) style |= Gdiplus::FontStyle::FontStyleBold;
	if (gs->italic) style |= Gdiplus::FontStyle::FontStyleItalic;

	Gdiplus::Font font(fontname.c_str(), gs->fontsize, style, Gdiplus::Unit::UnitPixel);
	graphics.MeasureString(wsz, len, &font, origin, &boundRect);

	*width = boundRect.Width;
	*height = boundRect.Height;

	delete[] wsz;

}

void BERTGraphicsDevice::drawCircle(double x, double y, double r, GraphicsStyle *gs) {

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

	Gdiplus::Pen stroke(RColor2ARGB(gs->col), gs->lwd);
	Gdiplus::SolidBrush fill(RColor2ARGB(gs->fill));

	Gdiplus::RectF rect(x - r, y - r, r * 2, r * 2);

	graphics.FillEllipse(&fill, rect);
	graphics.DrawEllipse(&stroke, rect);

	update();

}


void BERTGraphicsDevice::drawRect(double x1, double y1, double x2, double y2, GraphicsStyle *gs) {

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

	Gdiplus::Pen stroke(RColor2ARGB(gs->col), gs->lwd);
	Gdiplus::SolidBrush fill(RColor2ARGB(gs->fill));

	setPenOptions(stroke, gs);

	Gdiplus::REAL x = x1;
	Gdiplus::REAL y = y1;

	Gdiplus::REAL w = x2 - x1;
	Gdiplus::REAL h = y2 - y1;
	
	if (w < 0) { x = x2; w = -w; }
	if (h < 0) { y = y2; h = -h; }

	graphics.FillRectangle(&fill, x, y, w, h);
	graphics.DrawRectangle(&stroke, x, y, w, h);

	update();

}

void BERTGraphicsDevice::drawPoly(int n, double *x, double *y, int filled, GraphicsStyle *gs) {

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
	Gdiplus::Pen stroke(RColor2ARGB(gs->col), gs->lwd);

	setPenOptions(stroke, gs);

	if (n < 1) return;

	Gdiplus::PointF *pt = new Gdiplus::PointF[n];

	for (int i = 0; i < n; i++) {
		pt[i].X = x[i];
		pt[i].Y = y[i];
	}

	graphics.DrawLines(&stroke, pt, n);

	if (filled) {
		Gdiplus::SolidBrush fill(RColor2ARGB(gs->fill));
		graphics.FillPolygon(&fill, pt, n);
	}

	delete [] pt;
	update();

}

void BERTGraphicsDevice::drawLine(double x1, double y1, double x2, double y2, GraphicsStyle *gs) {

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

	Gdiplus::Pen stroke(RColor2ARGB(gs->col), gs->lwd);
	setPenOptions(stroke, gs);

	graphics.DrawLine(&stroke, (Gdiplus::REAL)x1, (Gdiplus::REAL)y1, (Gdiplus::REAL)x2, (Gdiplus::REAL)y2);
	update();

}

void BERTGraphicsDevice::drawBitmap(unsigned int* data, int pixel_width, int pixel_height, double x, double y, double target_width, double target_height, double rot) {

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

	// R data comes in 32-bit RGBA (lsb first byte-order).  There's no matching GDI+ format, 
	// so we have to munge the data.  when GDI+ says "ARGB" that means word order, or BGRA 
	// in lsb-first byte-order. we have to swap R and B.

	int len = pixel_width * pixel_height;
	unsigned int *pixels = new unsigned int[len];

	for (int i = 0; i < len; i++) {
		pixels[i] = (data[i] & 0xff00ff00) | ((data[i] >> 16) & 0x000000ff) | ((data[i] << 16) & 0x00ff0000);
	}

	// kind of ugly scaling, unfortunately.  may have to do this some other way.

	Gdiplus::Bitmap bmp(pixel_width, pixel_height, 4 * pixel_width, PixelFormat32bppARGB, (unsigned char*)pixels);

	if (target_height < 0) {
		y += target_height;
		target_height = -target_height;
	}

	if (target_width < 0) {
		x += target_width;
		target_width = -target_width;
	}

	Gdiplus::RectF rect(x, y, target_width, target_height);
	graphics.DrawImage(&bmp, rect);

	delete[] pixels;
	update();
}

void BERTGraphicsDevice::getCurrentSize(double &w, double &h) {

	// scale (FIXME: cache)
	HDC hdcScreen = ::GetDC(NULL);
	int logpixels = ::GetDeviceCaps(hdcScreen, LOGPIXELSX);
	::ReleaseDC(NULL, hdcScreen);

	CComPtr< Excel::Shape > shape;
	findDeviceTarget(name, shape);
	// if (!shape) createDeviceTarget(name, shape, width, height); // passing the old ones, I guess, in case it needs to be created
	
	if (shape) {
		Excel::IShape *ishape = (Excel::IShape*)(shape.p);
		if (ishape) {
			float fw, fh;
			ishape->get_Width(&fw);
			ishape->get_Height(&fh);

			/*
			// if there's a border, we need to subtract
			float lineWeight = 0;
			Excel::LineFormat *lf = 0;
			ishape->get_Line(&lf);
			if (lf) {
				lf->AddRef();
				lf->get_Weight(&lineWeight);
				lf->Release();
			}
			if (lineWeight) {
				fw -= (2 * lineWeight);
				fh -= (2 * lineWeight);
			}
			*/

			w = roundf(fw * logpixels / 72);
			h = roundf(fh * logpixels / 72);
		}
	}
	
	if (fabsf(w - width) >= 1 || fabsf(h - height) >= 1) {
		
		width = w;
		height = h;

		DebugOut("Resize graphics device -> %01.02f x %01.02f\n", width, height);

		if (pbitmap) {
			Gdiplus::Bitmap* bitmap = (Gdiplus::Bitmap*)pbitmap;
			delete bitmap;
		}

		Gdiplus::Bitmap *bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
		pbitmap = (void*)bitmap;
	}

}

void BERTGraphicsDevice::newPage(int color) {

	page++;

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	Gdiplus::SolidBrush bg(Gdiplus::Color( RColor2ARGB(color)));
	graphics.FillRectangle(&bg, 0, 0, width, height);
	update();

}

BERTGraphicsDevice::BERTGraphicsDevice( std::string &name, double w, double h ) :
	width(w), height(h), page(0), device(0), pbitmap(0), ref(0) {

	// defaults

	font_sans = FONT_SANS_DEFAULT;
	font_mono = FONT_MONO_DEFAULT;
	font_serif = FONT_SERIF_DEFAULT;

	int err = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name.length(), this->name, MAX_PATH);
	if (!err) {
		DWORD dwerr = GetLastError();
		DebugOut("MB ERR %d\n", dwerr);
	}

	WCHAR tempPath[MAX_PATH];
	GetTempPath(MAX_PATH, tempPath);
	GetTempFileName(tempPath, L"BERT_GraphicsTmp", 0, tempFile);

	Gdiplus::Bitmap *bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
	pbitmap = (void*)bitmap;

	dlist.push_back(this);

}

BERTGraphicsDevice::~BERTGraphicsDevice() {

	for (std::vector< BERTGraphicsDevice * > ::iterator iter = dlist.begin(); iter != dlist.end(); iter++) {
		if (*iter == this) {
			dlist.erase(iter);
			break;
		}
	}
	
	if (pbitmap) {
		Gdiplus::Bitmap* bitmap = (Gdiplus::Bitmap*)pbitmap;
		delete bitmap;
		pbitmap = 0;
	}

	DeleteFile(tempFile);
}
