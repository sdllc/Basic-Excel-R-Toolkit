
#
# constants
#

BERTXLL$EXCEL <- 1;
BERTXLL$CLOSECONSOLE <- 1023;

BERTXLL$CALLBACK <- "BERT_Callback";
BERTXLL$CloseConsole <- function(){ invisible(.Call(BERTXLL$CALLBACK, BERTXLL$CLOSECONSOLE, 0, PACKAGE=BERTXLL$MODULE )); };
BERTXLL$WordList <- function(){ 
	wl <- vector();
	for( i in search()){ wl <- c( wl, ls(i, all.names=1)); }
	wl;
}

#
# overload quit methods or it will stop the excel process
#

quit <- function(){ BERTXLL$CloseConsole() }
q <- quit;






