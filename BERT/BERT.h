// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the BERT_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// BERT_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef BERT_EXPORTS
#define BERT_API __declspec(dllexport)
#else
#define BERT_API __declspec(dllimport)
#endif

// This class is exported from the BERT.dll
class BERT_API CBERT {
public:
	CBERT(void);
	// TODO: add your methods here.
};

extern BERT_API int nBERT;

BERT_API int fnBERT(void);
