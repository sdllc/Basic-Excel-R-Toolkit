
#include "controlr.h"
#include "controlr_common.h"

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

  // I cannot figure out how to get R to print UTF8 when it has windows cp 
  // strings. it just seems to ignore all the things I set. temporarily let's
  // do this the hard way.

  // we can probably use fixed buffers here and expand, on the theory
  // that they won't get too large... if anything most console messages are 
  // too small, we should do some buffering (not here)

  const int32_t chunk = 256; // 32;

  static char *narrow_string = new char[chunk];
  static uint32_t narrow_string_length = chunk;

  static WCHAR *wide_string = new WCHAR[chunk];
  static uint32_t wide_string_length = chunk;

  int wide_size = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, buf, len, 0, 0);
  if (wide_size >= wide_string_length) {
    wide_string_length = chunk * (1 + (wide_size / chunk));
    delete[] wide_string;
    wide_string = new WCHAR[wide_string_length];
  }
  MultiByteToWideChar(CP_ACP, MB_COMPOSITE, buf, len, wide_string, wide_string_length);
  wide_string[wide_size] = 0;

  int narrow_size = WideCharToMultiByte(CP_UTF8, 0, wide_string, wide_size, 0, 0, 0, 0);
  if (narrow_size >= narrow_string_length) {
    narrow_string_length = chunk * (1 + (narrow_size / chunk));
    delete[] narrow_string;
    narrow_string = new char[narrow_string_length];
  }
  WideCharToMultiByte(CP_UTF8, 0, wide_string, wide_size, narrow_string, narrow_string_length, 0, 0);
  narrow_string[narrow_size] = 0;

  ConsoleMessage(narrow_string, narrow_size, flag);

  /*
  int size = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, buf, len, 0, 0);
  std::wstring utf16_str(size, '\0');
  MultiByteToWideChar(CP_ACP, MB_COMPOSITE, buf, len, &utf16_str[0], size);

  int utf8_size = WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(), utf16_str.length(), 0, 0, 0, 0);
  std::string utf8_str(utf8_size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(), utf16_str.length(), &utf8_str[0], utf8_size, 0, 0);

  ConsoleMessage(utf8_str.c_str(), utf8_str.length(), flag);
  */

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
