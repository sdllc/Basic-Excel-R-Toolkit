// BERT.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "BERT.h"
#include "ExcelFunctions.h"
#include "ThreadLocalStorage.h"

#include "RInterface.h"
#include "Dialogs.h"
#include "resource.h"
#include <Richedit.h>

std::vector < double > functionEntries;
std::list< std::string > loglist;

HWND hWndConsole = 0;

LPXLOPER12 BERTFunctionCall( 
	int index
	, LPXLOPER12 input_0
	, LPXLOPER12 input_1
	, LPXLOPER12 input_2
	, LPXLOPER12 input_3
	, LPXLOPER12 input_4
	, LPXLOPER12 input_5
	, LPXLOPER12 input_6
	, LPXLOPER12 input_7
	, LPXLOPER12 input_8
	, LPXLOPER12 input_9
	, LPXLOPER12 input_10
	, LPXLOPER12 input_11
	, LPXLOPER12 input_12
	, LPXLOPER12 input_13
	, LPXLOPER12 input_14
	, LPXLOPER12 input_15
	){

	XLOPER12 * rslt = get_thread_local_xloper12();
	resetXlOper( rslt );

	rslt->xltype = xltypeErr;
	rslt->val.err = xlerrName;

	if (index < 0 || index >= RFunctions.size()) return rslt;

	SVECTOR func = RFunctions[index];

	std::vector< LPXLOPER12 > args;

	if (func.size() > 1) args.push_back(input_0);
	if (func.size() > 2) args.push_back(input_1);
	if (func.size() > 3) args.push_back(input_2);
	if (func.size() > 4) args.push_back(input_3);
	if (func.size() > 5) args.push_back(input_4);
	if (func.size() > 6) args.push_back(input_5);
	if (func.size() > 7) args.push_back(input_6);
	if (func.size() > 8) args.push_back(input_7);
	if (func.size() > 9) args.push_back(input_8);
	if (func.size() > 10) args.push_back(input_9);
	if (func.size() > 11) args.push_back(input_10);
	if (func.size() > 12) args.push_back(input_11);
	if (func.size() > 13) args.push_back(input_12);
	if (func.size() > 14) args.push_back(input_13);
	if (func.size() > 15) args.push_back(input_14);
	if (func.size() > 16) args.push_back(input_15);

	RExec2(rslt, func[0], args);

	return rslt;
}

void logMessage(const char *buf, int len)
{
	std::string entry(buf);
	loglist.push_back(entry);
	while (loglist.size() > MAX_LOGLIST_SIZE) loglist.pop_front();

	if (hWndConsole)
	{
		/*
		std::string consolidated;
		for (std::list< std::string > ::iterator iter = loglist.begin(); iter != loglist.end(); iter++)
		{
			consolidated += iter->c_str();
			consolidated += "\r\n";
		}

		HWND hWnd = ::GetDlgItem(hWndConsole, IDC_LOG_WINDOW);
		if (hWnd)
		{
			::SetWindowTextA(hWnd, consolidated.c_str());
			::SendMessage(hWnd, WM_VSCROLL, SB_BOTTOM, (LPARAM)NULL);
		}
		*/

		AppendLog(buf);

	}
}

void resetXlOper(LPXLOPER12 x)
{
	if (x->xltype == xltypeStr && x->val.str)
	{
		delete[] x->val.str;
		x->val.str = 0;
	}
	else if (x->xltype == xltypeMulti && x->val.array.lparray)
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

short Reload()
{
	LoadStartupFile();
	MapFunctions();
	RegisterAddinFunctions();
	return 1;
}

short Configure()
{
	// ::MessageBox(0, L"No", L"Options", MB_OKCANCEL | MB_ICONINFORMATION);

	XLOPER12 xWnd;
	Excel12(xlGetHwnd, &xWnd, 0);

	// InitRichEdit();

	::DialogBox(ghModule,
		MAKEINTRESOURCE(IDD_DIALOG2),
		(HWND)xWnd.val.w,
		(DLGPROC)OptionsDlgProc);

	Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);

	return 1;
}

void ExcelStatus(const char *message)
{
	XLOPER12 xlUpdate;
	XLOPER12 xlMessage;

	xlUpdate.xltype = xltypeBool;

	if (!message)
	{
		xlUpdate.val.xbool = false;
		Excel12(xlcMessage, 0, 1, &xlUpdate );
	}
	else
	{
		int len = strlen(message) + 2;

		xlUpdate.val.xbool = true;

		xlMessage.xltype = xltypeStr | xlbitXLFree;
		xlMessage.val.str = new XCHAR[len];

		xlMessage.val.str[0] = len;
		for (int i = 0; i < len; i++) xlMessage.val.str[i + 1] = message[i];

		Excel12(xlcMessage, 0, 2, &xlUpdate, &xlMessage);
	}

}

void getLogText(std::string &str)
{
	str = "";
	for (std::list< std::string > ::iterator iter = loglist.begin(); iter != loglist.end(); iter++)
	{
		str += iter->c_str();
		str += "\r\n";
	}
}

void RExecStringBuffered(const char *buffer)
{
	PARSE_STATUS_2 ps2;
	int err;

	RExecString(buffer, &err, &ps2);
	if (ps2 == PARSE2_ERROR)
	{
		AppendLog("(parse error)\r\n");
	}
}

short Console()
{
	/*
	if (!hWndConsole)
	{
		XLOPER12 xWnd;
		Excel12(xlGetHwnd, &xWnd, 0);

		hWndConsole = ::CreateDialog(ghModule,
			MAKEINTRESOURCE(IDD_DIALOG1),
			(HWND)xWnd.val.w,
			(DLGPROC)ConsoleDlgProc);

		std::string consolidated;
		for (std::list< std::string > ::iterator iter = loglist.begin(); iter != loglist.end(); iter++)
		{
			consolidated += iter->c_str();
			consolidated += "\r\n";
		}

		HWND hWnd = ::GetDlgItem(hWndConsole, IDC_LOG_WINDOW);
		if (hWnd)
		{
			::SetWindowTextA(hWnd, consolidated.c_str());
			::SendMessage(hWnd, WM_VSCROLL, SB_BOTTOM, (LPARAM)NULL);
		}
		Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);
	}
	ShowWindow(hWndConsole, SW_SHOW);
	*/

	// ConsoleDlg(ghModule);

	XLOPER12 xWnd;
	Excel12(xlGetHwnd, &xWnd, 0);

	// InitRichEdit();

	::DialogBox( ghModule,
		MAKEINTRESOURCE(IDD_DIALOG1),
		(HWND)xWnd.val.w,
		(DLGPROC)ConsoleDlgProc);

	hWndConsole = 0;

	Excel12(xlFree, 0, 1, (LPXLOPER12)&xWnd);

	return 1;
}

void SetBERTMenu(bool add )
{
	XLOPER12 xl1, xlMenuName, xlMenu;
	XCHAR menuName[] = L" BERT";

	static bool menuInstalled = false;

	xl1.xltype = xltypeInt;
	xl1.val.w = 1;

	if (add)
	{
		if (menuInstalled) return;

		XCHAR menuEmpty[] = L" ";
		XCHAR menuEntry1[] = L" Options";
		XCHAR menuMacro1[] = L" BERT.Configure";
		XCHAR menuStatus1[] = L" Show configuration options";

		XCHAR menuEntry2[] = L" R Console";
		XCHAR menuMacro2[] = L" BERT.Console";
		XCHAR menuStatus2[] = L" Open the console";

		XCHAR menuEntry3[] = L" Reload Startup File";
		XCHAR menuMacro3[] = L" BERT.Reload";
		XCHAR menuStatus3[] = L" Reload Startup File";

		XCHAR menuEntry4[] = L" Install Packages";
		XCHAR menuMacro4[] = L" BERT.InstallPackages";
		XCHAR menuStatus4[] = L" Install Packages";

		xlMenu.xltype = xltypeMulti;
		xlMenu.val.array.columns = 4;
		xlMenu.val.array.rows = 5;
		xlMenu.val.array.lparray = new XLOPER12[ xlMenu.val.array.rows * xlMenu.val.array.columns ];

		int idx = 0;

		XLOPER12STR(xlMenu.val.array.lparray[0], menuName);
		XLOPER12STR(xlMenu.val.array.lparray[1], menuEmpty);
		XLOPER12STR(xlMenu.val.array.lparray[2], menuEmpty);
		XLOPER12STR(xlMenu.val.array.lparray[3], menuEmpty);

		XLOPER12STR(xlMenu.val.array.lparray[4], menuEntry1);
		XLOPER12STR(xlMenu.val.array.lparray[5], menuMacro1);
		XLOPER12STR(xlMenu.val.array.lparray[6], menuEmpty);
		XLOPER12STR(xlMenu.val.array.lparray[7], menuStatus1);

		XLOPER12STR(xlMenu.val.array.lparray[8], menuEntry2);
		XLOPER12STR(xlMenu.val.array.lparray[9], menuMacro2);
		XLOPER12STR(xlMenu.val.array.lparray[10], menuEmpty);
		XLOPER12STR(xlMenu.val.array.lparray[11], menuStatus2);

		XLOPER12STR(xlMenu.val.array.lparray[12], menuEntry3);
		XLOPER12STR(xlMenu.val.array.lparray[13], menuMacro3);
		XLOPER12STR(xlMenu.val.array.lparray[14], menuEmpty);
		XLOPER12STR(xlMenu.val.array.lparray[15], menuStatus3);

		XLOPER12STR(xlMenu.val.array.lparray[16], menuEntry4);
		XLOPER12STR(xlMenu.val.array.lparray[17], menuMacro4);
		XLOPER12STR(xlMenu.val.array.lparray[18], menuEmpty);
		XLOPER12STR(xlMenu.val.array.lparray[19], menuStatus4);

		Excel12( xlfAddMenu, 0, 2, &xl1, &xlMenu );

		delete[] xlMenu.val.array.lparray;
		menuInstalled = true;
	}
	else
	{
		if (!menuInstalled) return;

		XLOPER12STR(xlMenuName, menuName);
		Excel12(xlfDeleteMenu, 0, 2, &xl1, &xlMenuName);

		menuInstalled = false;
	}
}

LPXLOPER12 UpdateScript(LPXLOPER12 script)
{
	XLOPER12 * rslt = get_thread_local_xloper12();

	rslt->xltype = xltypeErr;
	rslt->val.err = xlerrValue;

	if (script->xltype == xltypeStr)
	{
		std::string str;
		int len = script->val.str[0];
		for (int i = 0; i < len; i++) str += (script->val.str[i + 1] & 0xff);

		if (!UpdateR(str))
		{
			rslt->xltype = xltypeBool;
			rslt->val.xbool = true;
			MapFunctions();
			RegisterAddinFunctions();
		}
		else
		{
			rslt->val.err = xlerrValue;
		}
	}

	return rslt;
}


void UnregisterFunctions()
{
	XLOPER xlRegisterID;

	for (std::vector< double > ::iterator iter = functionEntries.begin(); iter != functionEntries.end(); iter++ )
	{
		xlRegisterID.xltype = xltypeNum;
		xlRegisterID.val.num = *iter;
		Excel12(xlfUnregister, 0, 1, &xlRegisterID);
	}

	functionEntries.clear();

}


bool RegisterBasicFunctions()
{
	LPXLOPER12 xlParm[32];
	XLOPER12 xlRegisterID;

	int err;

	static bool fRegisteredOnce = false;

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
			int len = strlen(funcTemplates[i][j]);
			xlParm[j + 1]->xltype = xltypeStr;
			xlParm[j + 1]->val.str = new XCHAR[len + 2];

			//strcpy_s(xlParm[j + 1]->val.str + 1, len + 1, funcTemplates[i][j]);
			for (int k = 0; k < len; k++) xlParm[j + 1]->val.str[k + 1] = funcTemplates[i][j][k];

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

	// CRASHER for crash (recovery) testing // Excel4( xlFree, 0, 1, 1000 );

	fRegisteredOnce = true;
	return true;
}

bool RegisterAddinFunctions()
{
	LPXLOPER12 xlParm[32];
	XLOPER12 xlRegisterID;

	int err;

	static bool fRegisteredOnce = false;

	char szHelpBuffer[512] = " ";
	bool fExcel12 = false;

	// init memory

	for (int i = 0; i< 32; i++) xlParm[i] = new XLOPER12;

	// get the library; store as the first entry in our parameter list

	Excel12( xlGetName, xlParm[0], 0 );

	UnregisterFunctions();

	// { "BERTFunctionCall", "UU", "BERTTest", "Input", "1", "BERT", "", "100", "Test function", "", "", "", "", "", "", "" },

	int fcount = RFunctions.size();

	for (int i = 0; i< fcount && i< MAX_FUNCTION_COUNT; i++)
	{
		SVECTOR func = RFunctions[i];
		for (int j = 0; j < 15; j++)
		{
			switch (j)
			{
			case 0: sprintf_s(szHelpBuffer, 256, "BERTFunctionCall%04d", 1000 + i); break;
			case 1: 
				for (int k = 0; k < func.size(); k++) szHelpBuffer[k] = 'U';
				szHelpBuffer[func.size()] = 0;
				break;
			case 2: sprintf_s(szHelpBuffer, 256, "R.%s", func[0].c_str()); break;
			case 3: 
				szHelpBuffer[0] = 0;
				for (int k = 1; k < func.size(); k++)
				{
					if (strlen(szHelpBuffer)) strcat_s(szHelpBuffer, 256, ",");
					strcat_s(szHelpBuffer, 256, func[k].c_str());
				}
				break;
			case 4: sprintf_s(szHelpBuffer, 256, "1"); break;
			case 5: sprintf_s(szHelpBuffer, 256, ""); break;
			case 6: sprintf_s(szHelpBuffer, 256, "%d", 100 + i); break;
			case 7: sprintf_s(szHelpBuffer, 256, "Exported R function"); break;
			default: sprintf_s(szHelpBuffer, 256, ""); break;
			}
			
			int len = strlen(szHelpBuffer);
			xlParm[j + 1]->xltype = xltypeStr ;
			xlParm[j + 1]->val.str = new XCHAR[len + 2];

			//strcpy_s(xlParm[j + 1]->val.str + 1, len + 1, szHelpBuffer);
			for (int k = 0; k < len; k++) xlParm[j + 1]->val.str[k + 1] = szHelpBuffer[k];

			xlParm[j + 1]->val.str[0] = len;
		}

		xlRegisterID.xltype = xltypeMissing;
		err = Excel12v(xlfRegister, &xlRegisterID, 16, xlParm);
		if (xlRegisterID.xltype == xltypeNum)
		{
			functionEntries.push_back(xlRegisterID.val.num);
		}
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

	// CRASHER for crash (recovery) testing // Excel4( xlFree, 0, 1, 1000 );

	fRegisteredOnce = true;
	return true;
}

BFC(1000);
BFC(1001);
BFC(1002);
BFC(1003);
BFC(1004);
BFC(1005);
BFC(1006);
BFC(1007);
BFC(1008);
BFC(1009);
BFC(1010);
BFC(1011);
BFC(1012);
BFC(1013);
BFC(1014);
BFC(1015);
BFC(1016);
BFC(1017);
BFC(1018);
BFC(1019);
BFC(1020);
BFC(1021);
BFC(1022);
BFC(1023);
BFC(1024);
BFC(1025);
BFC(1026);
BFC(1027);
BFC(1028);
BFC(1029);
BFC(1030);
BFC(1031);
BFC(1032);
BFC(1033);
BFC(1034);
BFC(1035);
BFC(1036);
BFC(1037);
BFC(1038);
BFC(1039);
BFC(1040);
BFC(1041);
BFC(1042);
BFC(1043);
BFC(1044);
BFC(1045);
BFC(1046);
BFC(1047);
BFC(1048);
BFC(1049);
BFC(1050);
BFC(1051);
BFC(1052);
BFC(1053);
BFC(1054);
BFC(1055);
BFC(1056);
BFC(1057);
BFC(1058);
BFC(1059);
BFC(1060);
BFC(1061);
BFC(1062);
BFC(1063);
BFC(1064);
BFC(1065);
BFC(1066);
BFC(1067);
BFC(1068);
BFC(1069);
BFC(1070);
BFC(1071);
BFC(1072);
BFC(1073);
BFC(1074);
BFC(1075);
BFC(1076);
BFC(1077);
BFC(1078);
BFC(1079);
BFC(1080);
BFC(1081);
BFC(1082);
BFC(1083);
BFC(1084);
BFC(1085);
BFC(1086);
BFC(1087);
BFC(1088);
BFC(1089);
BFC(1090);
BFC(1091);
BFC(1092);
BFC(1093);
BFC(1094);
BFC(1095);
BFC(1096);
BFC(1097);
BFC(1098);
BFC(1099);

