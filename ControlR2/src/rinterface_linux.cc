
#include "controlr2_interface.h"
#include "controlr_common.h"
#include "convert.h"

extern "C" {
	extern void Rf_mainloop(void);
	extern void R_runHandlers(void *handlers, fd_set *readMask);
	extern void *R_InputHandlers;
	extern fd_set *R_checkActivity(int, int);
  extern int R_running_as_main_program; 
}

void RTick()
{
	R_runHandlers(R_InputHandlers, R_checkActivity(0, 0));
}

int RReadConsole(const char * prompt, unsigned char *buf, int len, int addtohistory){
	const char *cprompt = CHAR(STRING_ELT(GetOption1(install("continue")), 0));
	bool is_continuation = ( !strcmp( cprompt, prompt ));
  return InputStreamRead(prompt, reinterpret_cast<char*>(buf), len, addtohistory, is_continuation);
}

void RWriteConsoleEx(const char* message, int len, int status){
  ConsoleMessage(message, len, status);
}

void RShowMessage(const char *msg){
	std::cout << "RSM| " << msg << std::endl;
}

int RLoop(const char *rhome, const char *ruser, int argc, char ** argv){

  R_running_as_main_program = 1;

  // we can't use Rf_initEmbeddedR because we need
  // to set pointer (and set NULLs) in between calls
  // to Rf_initialize_R and setup_Rmainloop.

  Rf_initialize_R(argc, (char**)argv);

	ptr_R_WriteConsole = NULL;
	ptr_R_WriteConsoleEx = RWriteConsoleEx;
	ptr_R_ReadConsole = RReadConsole;   	
  ptr_R_ShowMessage = RShowMessage; // ??

  R_Outputfile = NULL;
  R_Consolefile = NULL;
	R_Interactive = TRUE;

  ///

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

  // also c-callable (FIXME: don't need this for com?) 
  R_RegisterCCallable("BERTControlR", "Callback", (DL_FUNC)RCallback);
  R_RegisterCCallable("BERTControlR", "COMCallback", (DL_FUNC)COMCallback);

  // now run the loop
  run_Rmainloop();

	Rf_endEmbeddedR(0);

}

void RSetUserBreak(const char *msg = 0){
  
  // if you're using pthreads, raise() calls pthread_kill(self), 
	// so don't use that.  just a heads up.

	kill( getpid(), SIGINT );	

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

  // FIXME: there has to be a linux equivalent of this. 
  // not obvious to me what it is. temp dummy
#ifdef _WIN32
  const char *version = getDLLVersion();
#else 
  const char *version = "3.4.4";
#endif
  if (!version) return;

  if (*version) *major = PartialVersion(&version);
  if (*version && *(++version)) *minor = PartialVersion(&version);
  if (*version && *(++version)) *patch = PartialVersion(&version);

}
