
#
# constants
#

BERTXLL$EXCEL <- 1;
BERTXLL$CLOSECONSOLE <- 1023;

BERTXLL$CALLBACK <- "BERT_Callback";
BERTXLL$CloseConsole <- function(){ invisible(.Call(BERTXLL$CALLBACK, BERTXLL$CLOSECONSOLE, 0, PACKAGE=BERTXLL$MODULE )); };
BERTXLL$Namespace <- function(a, b){ return( paste( b, "$", a, sep="" )); }
BERTXLL$WordList <- function(){ 
	wl <- vector();
	for( i in search()){ wl <- c( wl, ls(i, all.names=1)); }
	for( i in ls(.GlobalEnv))
	{
		if( is.environment(get(i, .GlobalEnv)))
		{
			wl <- c( wl, unlist( lapply( ls(get(i, .GlobalEnv), all.names=1), BERTXLL$Namespace, i)));	
		}
	}
	wl;
}

#
# overload quit methods or it will stop the excel process
#

quit <- function(){ BERTXLL$CloseConsole() }
q <- quit;






