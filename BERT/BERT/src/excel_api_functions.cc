
#include "stdafx.h"
#include "variable.pb.h"
#include "XLCALL.h"
#include "function_descriptor.h"
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

  XLOPER12 register_id;
  BERT *bert = BERT::Instance();
  for (auto entry : bert->function_list_) {
    register_id.xltype = xltypeNum;
    register_id.val.num = entry->register_id;
    Excel12(xlfUnregister, 0, 1, &register_id);
    entry->register_id = 0;
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
    entry->register_id = (int32_t)xlRegisterID.val.num;
    Excel12(xlFree, 0, 1, &xlRegisterID);

    for (int i = 1; i < 32; i++) {
      if (xlParm[i]->xltype & xltypeStr) delete[] xlParm[i]->val.str;
    }

    index++;
  }

  Excel12(xlFree, 0, 1, xlParm[0]);
  for (int i = 0; i < 32; i++) delete xlParm[i];
}

bool RegisterBasicFunctions()
{
  int err;
  XLOPER12 register_id;
  std::vector<LPXLOPER12> arguments;

  // here we're assuming that all strings are < 255 characters.
  // these are the static strings defined in the header, not 
  // anything user-defined.

  const int max_string_length = 256;

  for (int i = 0; i < 16; i++) arguments.push_back(new XLOPER12);
  for (int i = 1; i < 16; i++) {
    arguments[i]->xltype = xltypeStr;
    arguments[i]->val.str = new XCHAR[max_string_length];
  }

  // get the library; store as the first argument

  Excel12(xlGetName, arguments[0], 0);

  for (int i = 0; funcTemplates[i][0]; i++)
  {
    for (int j = 0; j < 15; j++)
    {
      int len = (int)wcslen(funcTemplates[i][j]);
      assert(len < (max_string_length - 1));

      wcscpy_s(&(arguments[j + 1]->val.str[1]), max_string_length - 1, funcTemplates[i][j]);
      arguments[j + 1]->val.str[0] = len;
    }

    register_id.xltype = xltypeMissing;
    err = Excel12v(xlfRegister, &register_id, 16, &(arguments[0]));
    Excel12(xlFree, 0, 1, &register_id);
  }

  Excel12(xlFree, 0, 1, arguments[0]);

  for (int i = 1; i < 16; i++) {
    delete[] arguments[i]->val.str;
  }

  for (auto argument : arguments) delete argument;

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


