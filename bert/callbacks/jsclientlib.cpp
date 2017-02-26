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

#include "../stdafx.h"

#include <Rconfig.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <R_ext/GraphicsEngine.h>

#ifdef length
#undef length
#endif

#include "./lodepng.h"
#include "./base64.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../BERTGDI.h"

using namespace std;

inline bool is_filled(int col) {
    const int alpha = R_ALPHA(col);
    return (col != NA_INTEGER) && (alpha != 0);
}

inline bool is_bold(int face) { return face == 2 || face == 4; }

inline bool is_italic(int face) { return face == 3 || face == 4; }

inline std::string fontname(const char* family_, int face) {
    
    std::string family(family_);
    if (face == 5) return "symbol";

    // we will pass symbolic names through, the client can
    // do translation if desired.  should be more flexible.
    // do clean up though.

    if( family == "mono" ) return "monospace";
    if (family == "sans" || family == "") return "sans-serif"; // default?
    return family;

}

const char *rgba( rcolor col ){

    static char color_buffer[64];
    static const char *none = "none";

    int alpha = R_ALPHA(col);

    if (col == NA_INTEGER || alpha == 0) {
        return none;
    } 
    if( alpha == 255.0 ){
        sprintf_s( color_buffer, "rgb(%d,%d,%d)", 
        R_RED(col), R_GREEN(col), R_BLUE(col));
    }
    else {	  
		sprintf_s( color_buffer, "rgba(%d,%d,%d,%01.02f)",
            R_RED(col), R_GREEN(col), R_BLUE(col), alpha/255.0 );
    }
    return color_buffer;	  

}

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

	return &gs;
}

/**
 * update: the style block is changed to look like this:
 *
 * style: {
 *   fill: string, 
 *   stroke: {
 *	   color: string, linewidth: num, cap: string, join: string
 *   }
 * }
 * 
 * with both fill and stroke being optional, and missing meaning
 * "don't fill" or "don't stroke".  instead of missing you can
 * set them to null, which might be easier to construct (but which
 * will waste processing time)
 *
 * actually, no you can't.  but you could set them to 0 or something,
 * which will test as false.  or false.
 */
const char * write_style_linetype( const pGEcontext gc, int filled) {

    static char buffer[256];
    static char fill[128];

    const char BUTT[] = "butt";
    const char ROUND[] = "round";
    const char BEVEL[] = "bevel";
    const char MITRE[] = "mitre";
    const char SQUARE[] = "square";
    
    const char *pcap = ROUND;
    const char *pjoin = ROUND;

    bool stroke = false;

    if( gc->col == NA_INTEGER || R_ALPHA(gc->col) == 0) {
		sprintf_s( buffer, "{ ");
    } 
    else {

        stroke = true;

        switch(gc->lend)
        {
        case GE_BUTT_CAP: pcap = BUTT; break;
        case GE_SQUARE_CAP: pcap = SQUARE; break;
        }

        switch(gc->ljoin)
        {
        case GE_BEVEL_JOIN: pjoin = BEVEL; break;
        case GE_MITRE_JOIN: pjoin = MITRE; break;
        }

//        sprintf_s( buffer, LITERAL({ "stroke": { "linewidth": %01.02f, "color": "%s", "cap": "%s", "join": "%s" }), 
//            gc->lwd / 96 * 72, rgba(gc->col), pcap, pjoin );
        
    }

    if( filled && is_filled(gc->fill)){
 //       sprintf_s( fill, LITERAL( %s "fill": "%s" }), stroke ? "," : "", rgba(gc->fill));
    }
//    else sprintf_s( fill, " } ");
//    strcat_s( buffer, fill );

    return buffer;
  
}

const char * fontdesc( const pGEcontext gc ){

    static char fontdesc[512];

	/*
    sprintf_s( fontdesc, "%s%s%.2fpx %s", 
        is_italic(gc->fontface) ? "italic " : "",
        is_bold(gc->fontface) ? "bold " : "",
        gc->cex * gc->ps,
        fontname(gc->fontfamily, gc->fontface).c_str()
    );
	*/

    return fontdesc;

}

std::vector< double > string_to_double_vector( const char * str ){

    char *end;
    char *dup = _strdup(str);
    std::vector< double > vec;
	char *context = 0;
	char *token = strtok_s( dup, ",", &context );
    while( token ){
        double tokenvalue = strtod( token, &end );
        vec.push_back( tokenvalue );
        token = strtok_s( 0, "," , &context);
    }
    free( dup );
    return vec;

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

	/*
    SEXP list = PROTECT( Rf_allocVector(STRSXP, 3));
    SEXP names = PROTECT( Rf_allocVector(STRSXP, 3));

    SET_STRING_ELT( list, 0, Rf_mkChar("font-metrics"));
    SET_STRING_ELT( list, 1, Rf_mkChar( str ));
    SET_STRING_ELT( list, 2, Rf_mkChar( fontdesc(gc) ));

    SET_STRING_ELT( names, 0, Rf_mkChar("command"));
    SET_STRING_ELT( names, 1, Rf_mkChar("text"));
    SET_STRING_ELT( names, 2, Rf_mkChar("font"));
    
    Rf_setAttrib( list, R_NamesSymbol, names );
    SEXP sexp = PROTECT( jsclient_callback_sync_( list ));

    if( sexp && Rf_isString( sexp )){
        std::vector< double > dx = string_to_double_vector(CHAR(STRING_ELT(sexp, 0)));
        if( dx.size() > 2 ){
            *ascent = dx[0];
            *descent = dx[1];
            *width = dx[2];
        }
    }

    UNPROTECT(3);

	*/
}

void set_clip(double x0, double x1, double y0, double y1, pDevDesc dd) {

}
 
void new_page(const pGEcontext gc, pDevDesc dd) {
    
    // does this command reset the display list? 
    // it might be useful to capture a snapshot
    // _before_ it's wiped...
    
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*)dd->deviceSpecific;						 
	pd->newPage();

	/*
    std::ostringstream os;
    
    os << LITERAL({ "cmd": "new-page", "device": ) << pd->device 
        << LITERAL(, "data": { "page": ) << pd->page 
        << LITERAL(, "height": ) << fixed << (double)(dd->bottom) 
        << LITERAL(, "width": ) << fixed << (double)(dd->right) 
        << LITERAL(, "background": ) QUOTE << rgba(dd->startfill) << QUOTE LITERAL( }} );

    pd->write(os.str().c_str());
	*/

    // increment for next call
    // pd->page++;

}

void close_device(pDevDesc dd) {
    
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;

	/*
    std::ostringstream os;
    
    os << LITERAL({ "cmd": "status", "device": ) << pd->device 
        << LITERAL(, "data": { "msg": "closing graphics device" }} );
    
    pd->write( os.str().c_str() );
	*/

    delete pd;
        
}

void draw_line(double x1, double y1, double x2, double y2, const pGEcontext gc, pDevDesc dd) {
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
	pd->drawLine(x1, y1, x2, y2, gsFromContext(gc));
}

void func_poly(int n, double *x, double *y, int filled, const pGEcontext gc, pDevDesc dd) {

    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;

	/*
    std::ostringstream os;

    os << LITERAL({ "cmd": "poly", "device": ) << pd->device 
        << LITERAL(, "data": { "style": ) << write_style_linetype(gc, filled)
        << LITERAL(, "points": [ );

    if( n ) os << "[" << x[0] << "," << y[0] << "]";
    for (int i = 1; i < n; i++) {
        os << ", [" << x[i] << "," << y[i] << "]";
    }

    os << LITERAL( ]}} );
    pd->write(os.str().c_str());
    */

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

    // TODO: maybe this is overoptimizing, but we can probably
    // do some memoization here: undoubtedly we're checking the
    // same strings and fonts over and over

    double width = 0;

		/*
    SEXP list = PROTECT( Rf_allocVector(STRSXP, 3));
    SEXP names = PROTECT( Rf_allocVector(STRSXP, 3));

    SET_STRING_ELT( list, 0, Rf_mkChar("measure-text"));
    SET_STRING_ELT( list, 1, Rf_mkChar( str ));
    SET_STRING_ELT( list, 2, Rf_mkChar( fontdesc(gc) ));

    SET_STRING_ELT( names, 0, Rf_mkChar("command"));
    SET_STRING_ELT( names, 1, Rf_mkChar("text"));
    SET_STRING_ELT( names, 2, Rf_mkChar("font"));
    
    Rf_setAttrib( list, R_NamesSymbol, names );
    SEXP sexp = PROTECT( jsclient_callback_sync_( list ));

    if( sexp ) width = Rf_asReal( sexp );

    UNPROTECT(3);
		*/

    return width;
}

void draw_rect(double x1, double y1, double x2, double y2,
              const pGEcontext gc, pDevDesc dd) {

    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
	pd->drawRect(x1, y1, x2, y2, gsFromContext(gc));

	/*
    std::ostringstream os;
    
    os << "{ \"cmd\": \"rect\", \"device\": " << pd->device << ", \"data\": "
        << "{ \"x1\": " << x1 
        << ", \"y1\": " << y1
        << ", \"x2\": " << x2
        << ", \"y2\": " << y2 
        << ", \"style\": " <<
        write_style_linetype(gc, true)
        << "}}";

    pd->write(os.str().c_str());
	*/

}

void draw_circle(double x, double y, double r, const pGEcontext gc,
                       pDevDesc dd) {

    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;
	/*
    std::ostringstream os;
    
    os << LITERAL({ "cmd": "circle", "device": ) << pd->device 
        << LITERAL(, "data": )
        << LITERAL({ "x": ) << x 
        << LITERAL(, "y": ) << y 
        << LITERAL(, "r": ) << r 
        << LITERAL(, "style": ) 
        << write_style_linetype( gc, true )
        << "}}";

    pd->write(os.str().c_str());
      */

}


void draw_text(double x, double y, const char *str, double rot,
              double hadj, const pGEcontext gc, pDevDesc dd) {

    BERTGraphicsDevice *pd = (BERTGraphicsDevice*) dd->deviceSpecific;

	/*
    std::ostringstream os;

    // escape the string suitable for utf8 json -- just 
    // quotes, I think (and slashes?)

    int i, len = strlen( str );
    std::string escaped;
    escaped.reserve( len * 1.5 );
    for( i = 0; i< len; i++ ){
        if( str[i] == '"' ) escaped += "\\";
        escaped += str[i];
    }
        
    os << LITERAL({ "cmd": "text", "device": ) << pd->device 
        << LITERAL(, "data": { "x": ) << x 
        << LITERAL(, "y": ) << y 
        << LITERAL(, "rot": ) << rot 
        << LITERAL(, "text": ) QUOTE << escaped << QUOTE
        << LITERAL(, "font": ) QUOTE << fontdesc(gc) << QUOTE
        << LITERAL(, "fill": ) QUOTE << rgba(gc->col) << QUOTE LITERAL( }} )

    ;

    pd->write(os.str().c_str());
    */

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

    // rot we can ignore; pass that to the client.  for our purposes
    // we will return the image at the original size; the client can
    // scale up.  (not sure which one is more efficient, but since 
    // the client is _rendering_, we should allow it to do all rendering
    // ops.  we are _encoding_.)
/*
    BERTGraphicsDevice *pd = (BERTGraphicsDevice*)dd->deviceSpecific;						 
    std::ostringstream os;

    // ? just let this pass...

    if (target_height < 0) target_height = -target_height;

    // raster data is 32-bit RGBA, although passed as int.  should be 
    // safe to cast (famous last words).

    std::string base64_str;
    std::vector<unsigned char> png;
    const unsigned char *pixels = (unsigned char*) raster;
    unsigned error = lodepng::encode( png, pixels, pixel_width, pixel_height );

    if( !error ) base64_str = base64_encode( reinterpret_cast< unsigned char* >(png.data()), png.size());

    os << "{\"cmd\": \"img\", \"device\": " << pd->device << ", \"data\":{ \"x\": "
        << x << ", \"y\": " << y 
        << ", \"rot\": " << rot 
        << ", \"width\": " << target_width
        << ", \"height\": " << target_height;

    if( !error ){		
        os << ", \"dataURL\": \"data:image/png;base64," << base64_str << QUOTE;
    }
    
    os << " }} ";

    pd->write(os.str().c_str());
*/
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
    dd->displayListOn = TRUE;
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

/**
 * sync callback function: returns a response as a SEXP
 * /
SEXP jsclient_callback_sync_( SEXP data, bool buffer) 
{
    static CALLBACK_FN_SYNC *callback = (CALLBACK_FN_SYNC*)R_GetCCallable("BERT", "CallbackSync");
    if( callback ) return callback( data, buffer );
    return R_NilValue;
}

/ **
 * async callback function
 * /
void jsclient_callback_( std::string channel, SEXP data, bool buffer = false) 
{
    static CALLBACK_FN_SEXP *callback = (CALLBACK_FN_SEXP*)R_GetCCallable("BERT", "CallbackSEXP");
    if( callback ) callback( channel.c_str(), data, buffer );
}

*/

/**
 * resize the graphics device, and optionally call replay
 */ 
void jsclient_device_resize_( int device, double width, double height ) // , bool replay ) 
{
    // FIXME: should check it's indeed our device
    
    pGEDevDesc gd = GEgetDevice( device-1 );
    if( gd ){
            
        gd->dev->right = width;
        gd->dev->bottom = height;
  
        // ... ?
        // if( replay ) GEplayDisplayList(gd);
    
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

