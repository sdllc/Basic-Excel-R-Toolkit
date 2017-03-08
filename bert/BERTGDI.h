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

#ifndef __BERT_GDI_H
#define __BERT_GDI_H

bool initGDIplus(bool startup);
void createDeviceTarget();

#define FONT_SANS_DEFAULT	L"Segoe UI"
#define FONT_MONO_DEFAULT	L"Consolas"
#define FONT_SERIF_DEFAULT	L"Palatino Linotype"

// the classes are largely to separate R and GDI+, which don't mix well.
// graphics style is a proxy for R_GE_gcontext.  actually as long as we're 
// doing that, we can add some useful information.

typedef struct {

	int col;
	int fill;
	double gamma;
	double lwd;
	int lty;
	int lend;
	int ljoin;
	double lmitre;
	double cex;
	double ps;
	double lineheight;
	int fontface;
	char fontfamily[201];

	// +

	bool bold;
	bool italic;
	double fontsize;

	std::string fontname;

} GraphicsStyle;

class BERTGraphicsDevice {
public:

	int page;
	int device;
	double width;
	double height;
	bool dirty;

	std::basic_string<WCHAR> font_sans;
	std::basic_string<WCHAR> font_mono;
	std::basic_string<WCHAR> font_serif;

	void *pbitmap;
	void *ref;

	WCHAR name[MAX_PATH]; // can this change? 

	// we need a temporary file for the device as well, unless we can 
	// figure out a way to just use pictures.  FIXME: extension?

	WCHAR tempFile[MAX_PATH];

public:

	BERTGraphicsDevice( std::string &name, double width, double height);
	~BERTGraphicsDevice(); 

public:

	void setDevice(int id) {
		device = id;
	}

	void newPage( int color = 0xffffffff );
//	void setSize(double width, double height);
	void drawLine(double x1, double y1, double x2, double y2, GraphicsStyle *gs);
	void drawRect(double x1, double y1, double x2, double y2, GraphicsStyle *gs);
	void drawPoly(int n, double *x, double *y, int filled, GraphicsStyle *gs);
	void drawCircle(double x, double y, double r, GraphicsStyle *gs);

	std::basic_string < WCHAR > mapFontName(std::string name);

	void measureText(const char *str, GraphicsStyle *gs, double *width, double *height);
	void drawText(const char *str, double x, double y, double rot, GraphicsStyle *gs);

	void drawBitmap(unsigned int* data, int pixel_width, int pixel_height, double x, double y, double target_width, double target_height, double rot);

//	void getDeviceSize(double &w, double &h);
	void getCurrentSize(double &width, double &height);

//	void flush();
	void repaint();
	void update();

};


#endif // #ifndef __BERT_GDI_H

