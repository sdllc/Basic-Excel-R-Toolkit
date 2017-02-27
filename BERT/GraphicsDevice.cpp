/**
 * Copyright (c) 2017 Structured Data LLC
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

#include "stdafx.h"

#include <Rconfig.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <R_ext/GraphicsEngine.h>

#ifdef length
#undef length
#endif

#include <string>
#include <iostream>

#include "BERTGDI.h"

using namespace std;

GraphicsStyle *gsFromContext(pGEcontext gc) {

	// this is definitely single-threaded, right?
	static GraphicsStyle gs;

	// we could in theory just memcpy, but there's no guarantee 
	// it's stable, so we should do this directly.

	gs.col = gc->col;
	gs.fill = gc->fill;
	gs.gamma = gc->gamma;
	gs.lwd= gc->lwd;
	gs.lty= gc->lty;
	gs.lend= gc->lend;
	gs.ljoin= gc->ljoin;
	gs.lmitre= gc->lmitre;
	gs.cex= gc->cex;
	gs.ps= gc->ps;
	gs.lineheight= gc->lineheight;
	gs.fontface= gc->fontface;
	memcpy(gs.fontfamily, gc->fontfamily, 201);

	// new stuff

	gs.filled = true; //  is_filled(gc->col);

	gs.italic = (gc->fontface == 3) || (gc->fontface == 4);
	gs.bold = (gc->fontface == 2) || (gc->fontface == 4);
	gs.fontsize = gc->cex * gc->ps;

	// basically pass through.  we can handle "mono", "monospace", "serif", "sans", 
	// "sans-serif" and "" (defaults to sans); otherwise we'll treat it as a font 
	// family name.

	if (gc->fontface == 5) gs.fontname = "symbol";
	else gs.fontname = gc->fontfamily;

	return &gs;
}

void get_metric_info( int c, const pGEcontext gc, double* ascent, double* descent, double* width, pDevDesc dd) {

    static char str[8];

    // this one can almost certainly do some caching.

    // Convert to string - negative implies unicode code point
    if (c < 0) {
        Rf_ucstoutf8(str, (unsigned int) -c);
    } else {
        str[0] = (char) c;
        str[1] = '\0';
    }

	double w, h;

	BERTGraphicsDevice *pd = (BERTGraphicsDevice*)dd->deviceSpecific;
	pd->measureText(str, gsFromContext(gc), &w, &h);

	// fudging a little

	*width = w;
	*ascent = h + .5;
	*descent = 0;
}

void set_clip(double x0, double x1, double y0, double y1, pDevDesc dd) {

	// ...

}
 
void new_page(const pGEcontext gc, pDevDesc dd) {
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*)dd->deviceSpecific;						 
	pd->newPage(dd->startfill);
}

void close_device(pDevDesc dd) {
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
    delete pd;
	dd->deviceSpecific = 0;
}

void draw_line(double x1, double y1, double x2, double y2, const pGEcontext gc, pDevDesc dd) {
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
	pd->drawLine(x1, y1, x2, y2, gsFromContext(gc));
}

void func_poly(int n, double *x, double *y, int filled, const pGEcontext gc, pDevDesc dd) {
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
	pd->drawPoly(n, x, y, filled, gsFromContext(gc));
}

void draw_polyline(int n, double *x, double *y, const pGEcontext gc, pDevDesc dd) {
    func_poly(n, x, y, 0, gc, dd);
}

void draw_polygon(int n, double *x, double *y, const pGEcontext gc, pDevDesc dd) {
    func_poly(n, x, y, 1, gc, dd);
}

void draw_path(double *x, double *y,
              int npoly, int *nper,
              Rboolean winding,
              const pGEcontext gc, pDevDesc dd) {
    
	cerr << "ENOTIMPL: draw_path (FIXME)" << endl;

}

double get_strWidth(const char *str, const pGEcontext gc, pDevDesc dd) {
	double w, h;
	BERTGraphicsDevice *pd = (BERTGraphicsDevice*)dd->deviceSpecific;
	pd->measureText(str, gsFromContext(gc), &w, &h);
	return w;
}

void draw_rect(double x1, double y1, double x2, double y2,
              const pGEcontext gc, pDevDesc dd) {
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
	pd->drawRect(x1, y1, x2, y2, gsFromContext(gc));
}

void draw_circle(double x, double y, double r, const pGEcontext gc,
                       pDevDesc dd) {
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
	pd->drawCircle(x, y, r, gsFromContext(gc));
}


void draw_text(double x, double y, const char *str, double rot,
              double hadj, const pGEcontext gc, pDevDesc dd) {
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
	pd->drawText(str, x, y, rot, gsFromContext(gc));
}

void get_size( double *left, double *right, double *bottom, double *top, pDevDesc dd) {
    *left = dd->left;
    *right = dd->right;
    *bottom = dd->bottom;
    *top = dd->top;
}

void draw_raster(unsigned int *raster, 
                 int pixel_width, int pixel_height,
                 double x, double y,
                 double target_width, double target_height,
                 double rot,
                 Rboolean interpolate,
                 const pGEcontext gc, pDevDesc dd) {

	BERTGraphicsDevice *pd = (BERTGraphicsDevice*)dd->deviceSpecific;
	pd->drawBitmap((unsigned int*)raster, pixel_width, pixel_height, x, y, target_width, target_height, rot);

}

/**
 * constructor, sets default options.  see [1]
 */
pDevDesc js_device_new( std::string &name, rcolor bg, double width, double height, int pointsize) {

    pDevDesc dd = (DevDesc*) calloc(1, sizeof(DevDesc));
    if (dd == NULL) return dd;
	
    dd->startfill = bg;
    dd->startcol = R_RGB(0, 0, 0);
    dd->startps = pointsize;
    dd->startlty = 0;
    dd->startfont = 1;
    dd->startgamma = 1;

    dd->activate = NULL;
    dd->deactivate = NULL;
  
    dd->newPage = new_page;
    dd->close = close_device;
    dd->clip = set_clip;
    dd->size = get_size;
    dd->metricInfo = get_metric_info;
    dd->strWidth = get_strWidth;

    dd->line = draw_line;
    dd->text = draw_text;
    dd->rect = draw_rect;
    dd->circle = draw_circle;
    dd->polygon = draw_polygon;
    dd->polyline = draw_polyline;
    dd->path = draw_path;
    dd->raster = draw_raster;
    dd->mode = NULL;
    dd->cap = NULL;

    dd->wantSymbolUTF8 = (Rboolean) 1;
    dd->hasTextUTF8 = (Rboolean) 1;
    dd->textUTF8 = draw_text;
    dd->strWidthUTF8 = get_strWidth;

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
	dd->displayListOn = FALSE; // FIXME: if you implement resize, we'll need this // TRUE;
    dd->haveTransparency = 2;
    dd->haveTransparentBg = 2;
    
    dd->haveRaster = 3; // yes, except for missing values

    dd->deviceSpecific = new BERTGraphicsDevice( name, width, height );
    return dd;
}

//=============================================================================
//
// exported functions 
//
//=============================================================================

void jsclient_device_resize_(int device, double width, double height, bool replay )
{
	// FIXME: should check it's indeed our device
	pGEDevDesc gd = GEgetDevice(device - 1);
	if (gd) {
		gd->dev->right = width;
		gd->dev->bottom = height;
		if (replay) GEplayDisplayList(gd);
	}
	else cerr << "Can't find device " << device << endl;

}

/**
 * create graphics device, add to display list 
 */ 
int jsclient_device_( std::string name, std::string background, double width, double height, int pointsize) 
{
    rcolor bg = R_GE_str2col(background.c_str());
    int device = 0;
  
    R_GE_checkVersionOrDie(R_GE_version);
    R_CheckDeviceAvailable();

    pDevDesc dev = js_device_new(name, bg, width, height, pointsize);
    pGEDevDesc gd = GEcreateDevDesc(dev);
    GEaddDevice2(gd, name.c_str());
    GEinitDisplayList(gd);
    device = GEdeviceNumber(gd) + 1; // to match what R says
    ((BERTGraphicsDevice*)(dev->deviceSpecific))->setDevice(device);

    // cout << "device number: " << device << endl;

    return device;
  
}

