.listfunctions <- function(){

	v <- vector();
	funclist <- lsf.str(.GlobalEnv);
	for( func in funclist ){
		v <- append( v, func );
	}
	v;

}

.listfunctionargs <- function(){

	rval = list();
	funclist <- lsf.str(.GlobalEnv);
	for( func in funclist )
	{
		arglist <- formals(func); # returns a pairlist
		if( length( arglist ) > 0 ){
			rval[[func]] = names(arglist);
		}
		else rval[[func]] = vector();
	}
	
	rval;
}


