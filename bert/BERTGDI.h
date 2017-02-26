
#ifndef __BERT_GDI_H
#define __BERT_GDI_H

bool initGDIplus(bool startup);
void createDeviceTarget();

// the classes are largely to separate R and GDI+, which don't mix well.
// graphics style is a proxy for R_GE_gcontext. 

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

} GraphicsStyle;

class BERTGraphicsDevice {
public:

	int page;
	int device;
	double width;
	double height;
	bool dirty;

	void *pbitmap;

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

	void newPage();
	void setSize(double width, double height);
	void drawLine(double x1, double y1, double x2, double y2, GraphicsStyle *gs);
	void drawRect(double x1, double y1, double x2, double y2, GraphicsStyle *gs);

	void getDeviceSize(double &w, double &h);

	void repaint();
	void update();

};


#endif // #ifndef __BERT_GDI_H

