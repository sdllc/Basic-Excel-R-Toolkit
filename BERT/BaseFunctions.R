
#
# constants
#

BERTXLL$CLOSE <- 1023;
BERTXLL$CALLBACK <- "BERT_Callback";

#
# methods
#

BERTXLL$.listfunctionargs <- function(env = .GlobalEnv){
	rval = list();
	funclist <- lsf.str(env);
	for( func in funclist )
	{
		if( func == "quit" || func == "q" ){}
		else {
			arglist <- formals(get(func, env)); # returns a pairlist
			if( length( arglist ) > 0 ){
				rval[[func]] = names(arglist);
			}
			else rval[[func]] = vector();
		}
	}
	rval;
}

BERTXLL$CloseConsole <- function(){ invisible(.Call(BERTXLL$CALLBACK, BERTXLL$CLOSE, 0, PACKAGE=BERTXLL$MODULE )); };

#
# overload quit methods or it will stop the excel process
#

quit <- function(){ BERTXLL$q() }
q <- quit;






