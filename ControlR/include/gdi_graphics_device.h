#pragma once

#define FONT_SANS_DEFAULT	    L"Segoe UI"
#define FONT_MONO_DEFAULT	    L"Consolas"
#define FONT_SERIF_DEFAULT	  L"Palatino Linotype"

namespace gdi_graphics_device {

  // this is clipped from r graphics, so we can use it without 
  // including that file (and all the files it includes), keeping
  // the namespace clean. this will of course break if that structure
  // changes.

  typedef enum {
    ROUND_CAP = 1,
    BUTT_CAP = 2,
    SQUARE_CAP = 3
  } LineEnd;

  typedef enum {
    ROUND_JOIN = 1,
    MITRE_JOIN = 2,
    BEVEL_JOIN = 3
  } LineJoin;
  
  typedef struct {
    int col;             /* pen colour (lines, text, borders, ...) */
    int fill;            /* fill colour (for polygons, circles, rects, ...) */
    double gamma;        /* Gamma correction */
    double lwd;          /* Line width (roughly number of pixels) */
    int lty;             /* Line type (solid, dashed, dotted, ...) */
    LineEnd lend;   /* Line end */
    LineJoin ljoin; /* line join */
    double lmitre;       /* line mitre */
    double cex;          /* Character expansion (font size = fontsize*cex) */
    double ps;           /* Font size in points */
    double lineheight;   /* Line height (multiply by font size) */
    int fontface;        /* Font face (plain, italic, bold, ...) */
    char fontfamily[201]; /* Font family */
  } GraphicsContext;

  class Device {
  private:

    std::string temp_file_path_;
    std::string name_;
    
    std::wstring font_sans_;
    std::wstring font_mono_;
    std::wstring font_serif_;

    int32_t width_;
    int32_t height_;
    void *bitmap_;
    bool dirty_;

    // std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter_;

  public:
    bool dirty() { return dirty_; }
    std::string image_path() { return temp_file_path_; }
    std::string name() { return name_; }
    int32_t width() { return width_; }
    int32_t height() { return height_; }

  public:
    Device(const std::string &name, int width, int height);
    ~Device();

  public:
    static bool InitGDIplus(bool startup = true);
    static void * GetPngCLSID();

  public:
    
    /** */
    void UpdateSize();

    /**
     * update indicates that we need an update, but doesn't actually paint. it will always 
     * trigger the next update; we might prefer that it do some window buffering.
     */
    void Update();

    /**
     * does the actual paint, which is kind of expensive. currently this is on a loop (from
     * the main IO thread), but it might need to get onto its own thread or use the windows
     * timer api (CreateTimerQueueTimer et al)
     * 
     * FIXME: notify excel.
     */
    void Repaint();

    /** 
     * generates font parameters. should generate an actual font struct, but we're isolating
     * gdiplus.
     */
    void MapFont(const GraphicsContext *context, std::wstring &font_name, int32_t &style, double &font_size);

    /**
     * measure text; optionally including ascent and descent
     */
    void MeasureText(const GraphicsContext *context, const char *text, double *width, double *height);
    void FontMetrics(const GraphicsContext *context, const std::string &text, double *ascent, double *descent, double *width);
    void RenderText(const GraphicsContext *context, const char *text, double x, double y, double rot);
    void NewPage(const GraphicsContext *context, int32_t width, int32_t height, uint32_t color);
    void DrawLine(const GraphicsContext *context, double x1, double y1, double x2, double y2);
    void DrawRect(const GraphicsContext *context, double x1, double y1, double x2, double y2);
    void DrawCircle(const GraphicsContext *context, double x, double y, double r);
    void DrawPoly(const GraphicsContext *context, int32_t n, double *x, double *y, int32_t filled);
    void DrawBitmap(unsigned int* data, int pixel_width, int pixel_height, double x, double y, double target_width, double target_height, double rot);

  };

}
