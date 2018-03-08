/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "controlr.h"
#include "controlr_common.h"

#include "convert.h"

/**
 * we're now basing "exec" commands on the standard repl; otherwise
 * we have to have two parallel paths for exec and debug.
 */
int R_ReadConsole(const char *prompt, char *buf, int len, int addtohistory) {

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

  // I cannot figure out how to get R to output UTF8 when it has windows cp 
  // strings. it just seems to ignore all the things I set. temporarily let's
  // do this the hard way.

  if (ValidUTF8(buf, len)) {
    return ConsoleMessage(buf, len, flag);
  }

  char *string;
  int length;

  WindowsCPToUTF8(buf, len, &string, &length);
  ConsoleMessage(string, length, flag);

}

/**
 * "ask ok" has no return value.  I guess that means "ask, then press OK",
 * not "ask if this is ok".
 *
 * FIXME: incorporate console client 
 */
void R_AskOk(const char *info) {
  ::MessageBoxA(0, info, "Message from R", MB_OK);
}

/**
 * 1 (yes) or -1 (no), I believe (based on #defines)
 *
 * FIXME: incorporate console client
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

/** call periodically to handle queued events / window messages */
void RTick() {
  R_ProcessEvents();
}

/** read a number from a version string (e.g. 3.4.1) */
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

/**
 * runs the main R loop; the rest of the code interacts via callbacks
 */
int RLoop(const char *rhome, const char *ruser, int argc, char ** argv) {

  Rstart Rp = new structRstart;

  char *local_rhome = new char[MAX_PATH];
  if(rhome) strcpy_s(local_rhome, MAX_PATH, rhome);
  else local_rhome[0] = 0;

  char *local_ruser = new char[MAX_PATH];
  if(ruser) strcpy_s(local_ruser, MAX_PATH, ruser);
  else local_ruser[0] = 0;

  R_setStartTime();
  R_DefParams(Rp);

  Rp->rhome = local_rhome;
  Rp->home = local_ruser;

  // typedef enum {RGui, RTerm, LinkDLL} UImode;
  // Rp->CharacterMode = LinkDLL;
  Rp->CharacterMode = RTerm;
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
  //R_set_command_line_arguments(0, 0);
  R_set_command_line_arguments(argc, argv);
  FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
  GA_initapp(0, 0);
  readconsolecfg();

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

  // also c-callable (FIXME: don't need this for com?) */
  R_RegisterCCallable("BERTControlR", "Callback", (DL_FUNC)RCallback);
  R_RegisterCCallable("BERTControlR", "COMCallback", (DL_FUNC)COMCallback);

  // now run the loop
  run_Rmainloop();

  // clean up
  delete[] local_ruser;
  delete[] local_rhome;

  delete Rp;

  Rf_endEmbeddedR(0);

  return 0;

}
