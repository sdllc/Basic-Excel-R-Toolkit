/*
 * controlR
 * Copyright (C) 2016 Structured Data, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "controlr_common.h"
#include "controlr_rinterface.h"

extern "C" {

  // instead of the "dlldo1" loop -- this one seems to be more stable
  extern void Rf_mainloop(void);

  extern void setup_Rmainloop();
  extern void run_Rmainloop();

  // in case we want to call these programatically (TODO)
  extern void R_RestoreGlobalEnvFromFile(const char *, Rboolean);
  extern void R_SaveGlobalEnvToFile(const char *);

  // for win32
  extern void R_ProcessEvents(void);

};

/**
 * we're now basing "exec" commands on the standard repl; otherwise
 * we have to have two parallel paths for exec and debug.
 */
int R_ReadConsole(const char *prompt, char *buf, int len, int addtohistory) {

  /*
  static bool final_init = false;
  if (!final_init) {
    final_init = true;
    R_RegisterCCallable("BERTControlR", "Callback", (DL_FUNC)RCallback);
    R_RegisterCCallable("BERTControlR", "COMCallback", (DL_FUNC)COMCallback);

   }
   */

  // every time?

  const char *cprompt = CHAR(STRING_ELT(GetOption1(install("continue")), 0));
  bool is_continuation = (!strcmp(cprompt, prompt));

  return InputStreamRead(prompt, buf, len, addtohistory, is_continuation);
}

/**
 * console messages are passed through.  note the signature is different on
 * windows/linux so implementation is platform dependent.
 */
void R_WriteConsoleEx(const char *buf, int len, int flag) {
  ConsoleMessage(buf, len, flag);
}

/**
 * "ask ok" has no return value.  I guess that means "ask, then press OK",
 * not "ask if this is ok".
 */
void R_AskOk(const char *info) {
  ::MessageBoxA(0, info, "Message from R", MB_OK);
}

/**
 * 1 (yes) or -1 (no), I believe (based on #defines)
 */
int R_AskYesNoCancel(const char *question) {
  return (IDYES == ::MessageBoxA(0, question, "Message from R", MB_YESNOCANCEL)) ? 1 : -1;
}

/** function pointer cannot be null */
void R_CallBack(void) {}

/** function pointer cannot be null */
void R_Busy(int which) {}

/**
 * break; on windows this is polled
 */
void RSetUserBreak(const char *msg) {

  // FIXME: synchronize (actually that's probably not helpful, unless we
  // can synchronize with whatever is clearing it inside R, which we can't)

  std::cout << "r-set-user-break" << std::endl;

  UserBreak = 1;
  //if( msg ) ConsoleMessage( msg, 0, 1 );
  //else ConsoleMessage("user break", 0, 1);

}

int PartialVersion(const char **ptr) {

  char buffer[32];
  memset(buffer, 0, 32);

  for (int i = 0; i < 32; i++, (*ptr)++) {
    char c = **ptr;
    if (!c || c == '.') break;
    buffer[i] = c;
  }

  return atoi(buffer);
}

void RGetVersion(int32_t *major, int32_t *minor, int32_t *patch) {

  *major = *minor = *patch = 0;

  const char *version = getDLLVersion();
  if (!version) return;

  if (*version) *major = PartialVersion(&version);
  if (*version && *(++version)) *minor = PartialVersion(&version);
  if (*version && *(++version)) *patch = PartialVersion(&version);

}

int RLoop(const char *rhome, const char *ruser, int argc, char ** argv) {

  structRstart rp;
  Rstart Rp = &rp;
  char Rversion[25];

  char RHome[MAX_PATH];
  char RUser[MAX_PATH];

  DWORD dwPreserve = 0;

  if (rhome) strcpy_s(RHome, rhome);
  if (ruser) strcpy_s(RUser, ruser);

  /*
  sprintf_s(Rversion, 25, "%s.%s", R_MAJOR, R_MINOR);
  if (strcmp(getDLLVersion(), Rversion) != 0) {
    cerr << "Error: R.DLL version does not match" << endl;
    return -1;
  }
  */

  R_setStartTime();
  R_DefParams(Rp);

  Rp->rhome = RHome;
  Rp->home = RUser;

  // typedef enum {RGui, RTerm, LinkDLL} UImode;
  Rp->CharacterMode = LinkDLL;
  Rp->R_Interactive = TRUE;

  Rp->ReadConsole = R_ReadConsole;
  Rp->WriteConsole = NULL;
  Rp->WriteConsoleEx = R_WriteConsoleEx;

  Rp->Busy = R_Busy;
  Rp->CallBack = R_CallBack;
  Rp->ShowMessage = R_AskOk;
  Rp->YesNoCancel = R_AskYesNoCancel;

  // we can handle these in code, more flexible
  Rp->RestoreAction = SA_NORESTORE;
  Rp->SaveAction = SA_NOSAVE;

  R_SetParams(Rp);
  R_set_command_line_arguments(0, 0);
  FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
  GA_initapp(0, 0);

  //Rf_mainloop();

  // call setup separately so we can install functions
  setup_Rmainloop();

  // install callbacks
  // FIXME: this is always static, is that required? does R just cache it? (...)
  static R_CallMethodDef methods[] = {
    { "BERT.Callback", (DL_FUNC)&RCallback, 2 },
    { "BERT.COMCallback", (DL_FUNC)&COMCallback, 5 },
    { 0, 0, 0 }
  };
  R_registerRoutines(R_getEmbeddingDllInfo(), NULL, methods, NULL, NULL);

  // also c-callable (FIXME: don't need this for com? */
  R_RegisterCCallable("BERTControlR", "Callback", (DL_FUNC)RCallback);
  R_RegisterCCallable("BERTControlR", "COMCallback", (DL_FUNC)COMCallback);

  // now run the loop
  run_Rmainloop();

  Rf_endEmbeddedR(0);

  return 0;

}

void RTick() {
  R_ProcessEvents();
}
