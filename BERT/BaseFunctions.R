
.listfunctionargs <- function(env = .GlobalEnv){
	rval = list();
	funclist <- lsf.str(env);
	for( func in funclist )
	{
		arglist <- formals(get(func, env)); # returns a pairlist
		if( length( arglist ) > 0 ){
			rval[[func]] = names(arglist);
		}
		else rval[[func]] = vector();
	}
	rval;
}


