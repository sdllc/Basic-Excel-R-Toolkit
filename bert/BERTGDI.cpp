
#include "stdafx.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "DebugOut.h"
#include "BERTGDI.h"

#include <vector>

extern HWND getExcelHWND();
extern LPDISPATCH pdispApp;

extern void jsclient_device_resize_(int dev, double width, double height);


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

#include "callbacks\mso.tlh"
#include "callbacks\excel.tlh"

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

	if (startup) {
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

void createDeviceTarget( const WCHAR *name, CComPtr< Excel::Shape > &target, double w, double h, int device ) {

	::DebugOut("creating device target\n");

	HDC hdcScreen = ::GetDC(NULL);
	int logpixels = ::GetDeviceCaps(hdcScreen, LOGPIXELSX);
	::ReleaseDC(NULL, hdcScreen);

	WCHAR wsz[32];
	wsprintf(wsz, L" (%d)", device);

	std::basic_string< WCHAR > alttext = L"BERT/R Graphics Device Target: ";
	alttext += name;
	alttext += wsz;

	if (pdispApp) {
		CComQIPtr< Excel::_Application > app(pdispApp);
		if (app) {
			IDispatchPtr pdispsheet = app->GetActiveSheet();
			CComQIPtr< Excel::_Worksheet > sheet(pdispsheet.GetInterfacePtr());
			if (sheet) {
				CComPtr< Excel::Shapes > shapes;
				sheet->get_Shapes(&shapes);
				if (shapes) {
					Excel::ShapePtr pshape = shapes->AddShape(Office::msoShapeRectangle, 100, 100, (float)(w * 72 / logpixels), (float)(h * 72 / logpixels ));
					pshape->PutName(name);
					pshape->PutAlternativeText(alttext.c_str());
					pshape->Line->PutVisible(Office::MsoTriState::msoFalse);

					CComQIPtr< Excel::Shape > shape(pshape.GetInterfacePtr());
					target = shape;
				}
			}
		}
	}

}

void findDeviceTarget(const WCHAR *name, CComPtr<Excel::Shape> &target, double w, double h, int device) {

	HRESULT hr = S_OK;

	if (pdispApp) {

		CComPtr<Excel::Sheets> sheets;
		CComQIPtr<Excel::_Application> app(pdispApp);
		if (app) app->get_Worksheets(&sheets);
		if (sheets) {
			long count = sheets->GetCount();
			for (long l = 1; l <= count; l++) {
				CComVariant var = l;
				LPDISPATCH pdispsheet;
				if (SUCCEEDED(sheets->get_Item(var, &pdispsheet))) {
					CComPtr<Excel::Shapes> shapes;
					CComQIPtr< Excel::_Worksheet > sheet(pdispsheet);
					if (sheet) sheet->get_Shapes(&shapes);
					if (shapes) {
						CComQIPtr< IEnumVARIANT > iterator(shapes->Get_NewEnum());
						if (iterator) {
							iterator->Reset();
							CComVariant item;
							ULONG fetched;
							for (; hr != S_FALSE;) {
								hr = iterator->Next(1, &item, &fetched);
								if (fetched != 1) break;
								if (item.vt != VT_DISPATCH) break;
								CComQIPtr< Excel::Shape > shape(item.pdispVal);
								if (shape) {
									_bstr_t _name = shape->GetName();
									CComBSTR bstrName(_name.GetBSTR());
									if (bstrName == name) {
										target = shape;
										break;
									}
								}
							}
							iterator->Reset();
						}
					}
				}
			}
		}

		if (!target) createDeviceTarget(name, target, w, h, device);

	}

}

void timer_callback(HWND hwnd, UINT msg, UINT_PTR id, DWORD dwtime) {

	::DebugOut("TPROC\n");
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
	UINT_PTR id = WM_USER + 123491;
	::SetTimer(hwndExcel, id, 100, (TIMERPROC)timer_callback);

}

/**
 * this is the actual paint op, which happens on timer events.
 */
void BERTGraphicsDevice::repaint() {

	dirty = false;
	::DebugOut(" ** graphics update\n");

	CLSID pngClsid;
	GetEncoderClsid(L"image/png", &pngClsid);
	((Gdiplus::Bitmap*)pbitmap)->Save(tempFile, &pngClsid, NULL);
	
	CComPtr< Excel::Shape > shape;
	findDeviceTarget(name, shape, width, height, device);

	if (shape) {
		Excel::FillFormatPtr fill = shape->GetFill();
		if (fill) {
			fill->UserPicture(tempFile);
			// fill->Release();
		}
	}

}

void BERTGraphicsDevice::getDeviceSize(double &w, double &h) {

	w = width;
	h = height;

	CComPtr< Excel::Shape > shape;
	findDeviceTarget(name, shape, width, height, device); // passing the old ones, I guess, in case it needs to be created

	if (shape) {
		w = shape->GetWidth();
		h = shape->GetHeight();
	}

}

void BERTGraphicsDevice::drawLine(double x1, double y1, double x2, double y2, GraphicsStyle *gs) {

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	Gdiplus::Pen black(Gdiplus::Color(255, 0, 0, 0), gs->lwd);
	graphics.DrawLine(&black, (Gdiplus::REAL)x1, (Gdiplus::REAL)y1, (Gdiplus::REAL)x2, (Gdiplus::REAL)y2);

	update();

}

void BERTGraphicsDevice::drawRect(double x1, double y1, double x2, double y2, GraphicsStyle *gs) {

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	Gdiplus::Pen black(Gdiplus::Color(255, 0, 0, 0), gs->lwd);

	Gdiplus::REAL x = x1;
	Gdiplus::REAL y = y1;

	Gdiplus::REAL w = x2 - x1;
	Gdiplus::REAL h = y2 - y1;
	
	if (w < 0) { x = x2; w = -w; }
	if (h < 0) { y = y2; h = -h; }

	graphics.DrawRectangle(&black, x, y, w, h);

	update();

}


void BERTGraphicsDevice::newPage() {

	page++;

	// check size
	double w, h;
	getDeviceSize(w, h);

	// scale (FIXME: cache)
	HDC hdcScreen = ::GetDC(NULL);
	int logpixels = ::GetDeviceCaps(hdcScreen, LOGPIXELSX);
	::ReleaseDC(NULL, hdcScreen);

	w = w * logpixels / 72;
	h = h * logpixels / 72;

	::DebugOut("New page; device size reports %01.02f, %01.02f (we think it's %01.02f, %01.02f)", w, h, width, height);

	if (fabsf(w - width) >= 1 || fabsf(h - height) >= 1) {
		setSize(w, h);
	}

	Gdiplus::Graphics graphics((Gdiplus::Bitmap*)pbitmap);
	Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
	graphics.FillRectangle(&white, 0, 0, width, height);

	update();

}

void BERTGraphicsDevice::setSize(double width, double height) {

	if (this->width == width && this->height == height) return;
	if (pbitmap) {
		Gdiplus::Bitmap* bitmap = (Gdiplus::Bitmap*)pbitmap;
		delete bitmap;
	}

	this->width = width;
	this->height = height;

	jsclient_device_resize_(device, this->width, this->height);

	Gdiplus::Bitmap *bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
	pbitmap = (void*)bitmap;

	// replay?

}

BERTGraphicsDevice::BERTGraphicsDevice( std::string &name, double w, double h ) :
	width(w), height(h), page(0), device(0), pbitmap(0) {

	int err = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name.length(), this->name, MAX_PATH);
	if (!err) {
		DWORD dwerr = GetLastError();
		::DebugOut("MB ERR %d\n", dwerr);
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


	if (pbitmap) delete pbitmap;
	DeleteFile(tempFile);
}
