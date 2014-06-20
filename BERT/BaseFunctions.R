
BERT = new.env();

BERT$.listfunctionargs <- function(env = .GlobalEnv){
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


#BERT$DLL <- "BERT-32-D.xll";
BERT$CLOSE <- 1023;
BERT$CALLBACK <- "BERT_Callback";

#BERT$q <- function(){ invisible(.Call(BERT$CALLBACK, BERT$CLOSE, 0, PACKAGE=BERT$DLL )); };
BERT$q <- function(){ invisible(.Call(BERT$CALLBACK, BERT$CLOSE, 0 )); };

quit <- function(){ BERT$q() }
q <- quit;






