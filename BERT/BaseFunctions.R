
#
# constants
#

BERTXLL$EXCEL <- 1;
BERTXLL$CLOSECONSOLE <- 1023;

BERTXLL$CALLBACK <- "BERT_Callback";
BERTXLL$CloseConsole <- function(){ invisible(.Call(BERTXLL$CALLBACK, BERTXLL$CLOSE, 0, PACKAGE=BERTXLL$MODULE )); };

#
# overload quit methods or it will stop the excel process
#

quit <- function(){ BERTXLL$CloseConsole() }
q <- quit;






