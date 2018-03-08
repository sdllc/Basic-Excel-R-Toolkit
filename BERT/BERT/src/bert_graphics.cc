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

#include "stdafx.h"
#include "variable.pb.h"
#include "bert_graphics.h"

#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>

#include <codecvt>

#include "excel_com_type_libraries.h"

namespace BERTGraphics {
  
  void CreateDeviceTarget(LPDISPATCH appplication_pointer, const std::string &name, CComPtr< Excel::Shape > &target, double w, double h) {

    HRESULT hr;

    std::string compound_name = "BGD_";
    compound_name += name;

    DebugOut("creating device target\n");

    HDC hdcScreen = ::GetDC(NULL);
    int logpixels = ::GetDeviceCaps(hdcScreen, LOGPIXELSX);
    ::ReleaseDC(NULL, hdcScreen);

    std::string alttext = "BERT/R Graphics Device Target: ";
    alttext += name;

    if (appplication_pointer) {
      CComQIPtr< Excel::_Application > app(appplication_pointer);
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

                ishapes->raw_AddShape(Office::msoShapeRectangle, 100, 100, (float)(w * 72 / logpixels), (float)(h * 72 / logpixels), &shape);

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

  void FindDeviceTarget(LPDISPATCH application_dispatch, const std::string &name, CComPtr<Excel::Shape> &target) {

    HRESULT hr = S_OK;

    std::string compound_name = "BGD_";
    compound_name += name;

    if (application_dispatch) {

      CComPtr<Excel::Workbooks> workbooks;
      CComQIPtr<Excel::_Application> app(application_dispatch);

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
                      if (SUCCEEDED(ishapes->raw_Item(cvItem, &pshape))) {
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

  bool QuerySize(const std::string &name, BERTBuffers::CallResponse &response, LPDISPATCH application_dispatch) {
    
    CComPtr< Excel::Shape > shape;
    FindDeviceTarget(application_dispatch, name, shape);
    if (shape) {
      Excel::IShape *ishape = (Excel::IShape*)(shape.p);
      if (ishape) {
    
        HDC hdcScreen = ::GetDC(NULL);
        int logpixels = ::GetDeviceCaps(hdcScreen, LOGPIXELSX);
        ::ReleaseDC(NULL, hdcScreen);

        float fw, fh;
        ishape->get_Width(&fw);
        ishape->get_Height(&fh);

        auto graphics = response.mutable_result()->mutable_graphics();

        graphics->set_command(BERTBuffers::GraphicsUpdateCommand::query_size);
        graphics->set_width(roundf(fw * logpixels / 72));
        graphics->set_height(roundf(fh * logpixels / 72));
        
        return true;
      }
    }

    response.mutable_result()->set_boolean(false);
    return false;

  }

  void UpdateGraphics(const BERTBuffers::GraphicsUpdate &graphics, LPDISPATCH application_dispatch) {

    CComPtr< Excel::Shape > shape;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    //std::wstring wide_name = converter.from_bytes(name);

    FindDeviceTarget(application_dispatch, graphics.name(), shape);
    if (!shape) CreateDeviceTarget(application_dispatch, graphics.name(), shape, graphics.width(), graphics.height());

    if (shape) {

      CComPtr< Excel::FillFormat > fill;
      Excel::IShape *ishape = (Excel::IShape*)(shape.p);
      if (ishape) ishape->get_Fill(&fill);

      std::wstring wide_path = converter.from_bytes(graphics.path());
      if (fill) fill->UserPicture(wide_path.c_str());

    }

  }

}