// BERT.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "BERT.h"


// This is an example of an exported variable
BERT_API int nBERT=0;

// This is an example of an exported function.
BERT_API int fnBERT(void)
{
	return 42;
}

// This is the constructor of a class that has been exported.
// see BERT.h for the class definition
CBERT::CBERT()
{
	return;
}
