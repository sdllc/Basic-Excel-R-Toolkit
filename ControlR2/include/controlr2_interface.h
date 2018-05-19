
#ifndef _CONTROLR2_INTERFACE_H
#define _CONTROLR2_INTERFACE_H

/**
 * called by the R read console method (see comments for rationale)
 */
int InputStreamRead(const char *prompt, char *buf, int len, int addtohistory, bool is_continuation);

/**
 * handler for messages coming from the R side (js client package)
 */
void DirectCallback(const char *channel, const char *data, bool buffer);

/**
 * console output handler
 */
void ConsoleMessage(const char *buf, int len = -1, int flag = 0);

/** */
int RLoop(const char *rhome, const char *ruser, int argc, char ** argv);

#endif // #ifndef _CONTROLR2_INTERFACE_H
