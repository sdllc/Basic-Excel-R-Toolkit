
#include "stdafx.h"
#include "variable.pb.h"
#include "XLCALL.h"
#include "function_description.h"
#include "bert.h"
#include "basic_functions.h"
#include "type_conversions.h"
#include "windows_api_functions.h"

void resetXlOper(LPXLOPER12 x)
{
	if (x->xltype == (xltypeStr | xlbitDLLFree) && x->val.str)
	{
		// we pass a static string with zero length -- don't delete that

		if (x->val.str[0]) delete[] x->val.str;
		x->val.str = 0;

	}
	else if ((x->xltype == xltypeMulti || x->xltype == (xltypeMulti | xlbitDLLFree)) && x->val.array.lparray)
	{
		// have to consider the case that there are strings
		// in the array, or even nested multis (not that that
		// is a useful thing to do, but it could happen)

		int len = x->val.array.columns * x->val.array.rows;
		for (int i = 0; i < len; i++) resetXlOper(&(x->val.array.lparray[i]));

		delete[] x->val.array.lparray;
		x->val.array.lparray = 0;

	}

	x->val.err = xlerrNull;
	x->xltype = xltypeNil;

}

void UnregisterFunctions() {

	XLOPER12 xlRegisterID;
	BERT *bert = BERT::Instance();
	for (auto entry : bert->function_list_) {
		xlRegisterID.xltype = xltypeNum;
		xlRegisterID.val.num = entry->registerID;
		Excel12(xlfUnregister, 0, 1, &xlRegisterID);
		entry->registerID = 0;
	}
}

void RegisterFunctions() {

	LPXLOPER12 xlParm[32];
	XLOPER12 xlRegisterID;
	int err;

	BERT *bert = BERT::Instance();
	for (int i = 0; i < 32; i++) {
		xlParm[i] = new XLOPER12;
		xlParm[i]->xltype = xltypeMissing;
	}

	Excel12(xlGetName, xlParm[0], 0);
	int index = 1000;

	for (auto entry : bert->function_list_) {

		std::stringstream ss;

		ss.clear(); 
		ss.str("");
		ss << "BERTFunctionCall" << index;
		Convert::StringToXLOPER(xlParm[1], ss.str(), false);
		Convert::StringToXLOPER(xlParm[2], "UQQQQQQQQQQQQQQQQ", false);

		ss.clear();
		ss.str("");
		ss << "R." << entry->name;
		Convert::StringToXLOPER(xlParm[3], ss.str(), false);

		ss.clear();
		ss.str("");
		for (auto arg : entry->arguments) ss << ", " << arg->name;
		Convert::StringToXLOPER(xlParm[4], ss.str().c_str() + 2, false);

		Convert::StringToXLOPER(xlParm[5], "1", false);
		Convert::StringToXLOPER(xlParm[6], "Exported R Functions", false);

		ss.clear();
		ss.str("");
		ss << index;
		Convert::StringToXLOPER(xlParm[8], ss.str(), false);

		Convert::StringToXLOPER(xlParm[9], "Exported Function", false);

		// { L"BERT_Call", L"UQQQQQQQQQQQQQQQQ", L"BERT.Call", L"R Function, Argument", L"1", L"BERT", L"", L"99", L"Test BERT", L"", L"", L"", L"", L"", L"", L"" },

		xlRegisterID.xltype = xltypeMissing;
		err = Excel12v(xlfRegister, &xlRegisterID, 16, xlParm);
		entry->registerID = xlRegisterID.val.num;
		Excel12(xlFree, 0, 1, &xlRegisterID);

		for (int i = 1; i < 32; i++) {
			if(xlParm[i]->xltype & xltypeStr) delete[] xlParm[i]->val.str;
		}

		index++;
	}

	Excel12(xlFree, 0, 1, xlParm[0]);
	for (int i = 0; i< 32; i++) delete xlParm[i];
}

bool RegisterBasicFunctions()
{
	LPXLOPER12 xlParm[32];
	XLOPER12 xlRegisterID;

	int err;
	static bool fRegisteredOnce = false; // not used?

	char szHelpBuffer[512] = " ";
	bool fExcel12 = false;

	// init memory

	for (int i = 0; i< 32; i++) xlParm[i] = new XLOPER12;

	// get the library; store as the first entry in our parameter list

	Excel12(xlGetName, xlParm[0], 0);

	for (int i = 0; funcTemplates[i][0]; i++)
	{
		for (int j = 0; j < 15; j++)
		{
			int len = wcslen(funcTemplates[i][j]);
			xlParm[j + 1]->xltype = xltypeStr;
			xlParm[j + 1]->val.str = new XCHAR[len + 2];

			// strcpy_s(xlParm[j + 1]->val.str + 1, len + 1, funcTemplates[i][j]);
			// for (int k = 0; k < len; k++) xlParm[j + 1]->val.str[k + 1] = funcTemplates[i][j][k];

			wcscpy_s(&(xlParm[j + 1]->val.str[1]), len + 1, funcTemplates[i][j]);

			xlParm[j + 1]->val.str[0] = len;
		}

		xlRegisterID.xltype = xltypeMissing;
		err = Excel12v(xlfRegister, &xlRegisterID, 16, xlParm);

		Excel12(xlFree, 0, 1, &xlRegisterID);

		for (int j = 0; j < 15; j++)
		{
			delete[] xlParm[j + 1]->val.str;
		}

	}

	// clean up (don't forget to free the retrieved dll xloper in parm 0)

	Excel12(xlFree, 0, 1, xlParm[0]);

	for (int i = 0; i< 32; i++) delete xlParm[i];


	// debugLogf("Exit registerAddinFunctions\n");

	// set state and return

	// CRASHER for crash (recovery) testing // Excel12( xlFree, 0, 1, 1000 );

	fRegisteredOnce = true;
	return true;
}


BOOL WINAPI xlAutoOpen(void)
{
	RegisterBasicFunctions();
    BERT *bert = BERT::Instance();
	bert->Init();
	bert->MapFunctions();
	RegisterFunctions();
	return true;
}

void WINAPI xlAutoFree12(LPXLOPER12 pxFree)
{
	if (pxFree->xltype == (xltypeMulti | xlbitDLLFree)
		|| pxFree->xltype == (xltypeStr | xlbitDLLFree)) resetXlOper(pxFree);
}

BOOL WINAPI xlAutoClose(void)
{
	BERT::Instance()->Close();
	return true;
}

LPXLOPER12 WINAPI xlAddInManagerInfo12(LPXLOPER12 action)
{
	static XLOPER12 info, int_action;
	static XCHAR name[] = L" Basic Excel R Toolkit (BERT)";

	XLOPER12 type;
	type.xltype = xltypeInt;
	type.val.w = xltypeInt; // ??

	Excel12(xlCoerce, &int_action, 2, action, &type);

	if (int_action.val.w == 1)
	{
		info.xltype = xltypeStr;
		name[0] = (XCHAR)(wcslen(name + 1));
		info.val.str = name;
	}
	else
	{
		info.xltype = xltypeErr;
		info.val.err = xlerrValue;
	}

	return (LPXLOPER12)&info;
}


