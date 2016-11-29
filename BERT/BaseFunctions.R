
with( BERT, {

#========================================================
# constants
#========================================================

.EXCEL <- 1;

.ADD_USER_BUTTON <- 100;
.CLEAR_USER_BUTTONS <- 101;

.HISTORY <- 200;
.REMAP_FUNCTIONS <- 300;

.WATCHFILES <- 1020;
.CLEAR <- 1021;
.RELOAD <- 1022;
.CLOSECONSOLE <- 1023;

.CALLBACK <- "BERT_Callback";
.COM_CALLBACK <- "BERT_COM_Callback";

#========================================================
# functions - for general use
#========================================================

#--------------------------------------------------------
# clear buffer.  if you want, you can map this to a 
# 'clear()' function.
#--------------------------------------------------------
ClearBuffer <- function(){ invisible(.Call(.CALLBACK, .CLEAR, 0, PACKAGE=.MODULE )); };

#--------------------------------------------------------
# close the console
#--------------------------------------------------------
CloseConsole <- function(){ invisible(.Call(.CALLBACK, .CLOSECONSOLE, 0, PACKAGE=.MODULE )); };

#--------------------------------------------------------
# reload the startup file
#--------------------------------------------------------
ReloadStartup <- function(){ .Call(.CALLBACK, .RELOAD, 0, PACKAGE=.MODULE ); };

#========================================================
# explicit function registration
#========================================================

.function.map <- new.env();

#--------------------------------------------------------
# map all functions in an environment.  the ... arguments
# are passed to ls(), so use pattern='X' to subset 
# functions in the environment. 
#--------------------------------------------------------
UseEnvironment <- function(env, prefix, category, ...){
	count <- 0;
	if( missing( prefix )){ prefix = ""; }
	else { prefix = paste0( prefix, "." ); }
	if( missing( category )){ category = ""; }
	if(is.character(env)){ env = as.environment(env); }
	lapply( ls( env, ... ), function( name ){
		if( is.function( get( name, envir=env ))){
			fname <- paste0( prefix, name );
			assign( fname, list( name=fname, expr=name, envir=env, category=category ), envir=.function.map );
			count <<- count + 1;
		}
	});
	if( count > 0 ){ .Call( .CALLBACK, .REMAP_FUNCTIONS, 0, PACKAGE=.MODULE ); }
	return( count > 0 );
}

#--------------------------------------------------------
# this is an alias for UseEnvironment that prepends the
# package: for convenience.
#--------------------------------------------------------
UsePackage <- function( pkg, prefix, category, ... ){
	require( pkg );
	UseEnvironment( paste0( "package:", pkg ), prefix, category, ... );
}

#--------------------------------------------------------
# remove mapped environment/package functions
#--------------------------------------------------------
ClearMappedFunctions <- function(){
	rm( list=ls(.function.map), envir=.function.map );
	.Call( .CALLBACK, .REMAP_FUNCTIONS, 0, PACKAGE=.MODULE );
}

#========================================================
# API for user buttons
#========================================================

.UserButtonCallbacks <- list();
.UserButtonCallback <- function(index){
	return(.UserButtonCallbacks[[index+1]]$FUN());
}

#--------------------------------------------------------
# add a user button.  these are added to the ribbon,
# there's a max of 6.  callback is an R function.
#--------------------------------------------------------
AddUserButton <- function( label, FUN, imageMso = NULL ){

	ubc <- structure( list(), class = "UserButtonCallback" );
	ubc$label <- label;
	ubc$FUN <- FUN;
	ubc$imageMso <- imageMso;

	.Call(.CALLBACK, .ADD_USER_BUTTON, c( label, "BERT$.UserButtonCallback", imageMso ), 0, PACKAGE=.MODULE);

	len <- length( .UserButtonCallbacks );
	.UserButtonCallbacks[[len+1]] <<- ubc;
}

#--------------------------------------------------------
# list user buttons
#--------------------------------------------------------
ListUserButtons <- function(){
	print( .UserButtonCallbacks );
}

#--------------------------------------------------------
# remove user buttons
#--------------------------------------------------------
ClearUserButtons <- function(){
	.UserButtonCallbacks <<- list();
	.Call(.CALLBACK, .CLEAR_USER_BUTTONS, 0, 0, PACKAGE=.MODULE);
}

#========================================================
# API for the file watch utility
#========================================================

.WatchedFiles <- new.env();

.RestartWatch <- function(){
	rslt <- .Call( BERT$.CALLBACK, BERT$.WATCHFILES, ls(.WatchedFiles), 0, PACKAGE=BERT$.MODULE );
	if( !rslt ){
		cat( "File watch failed.  Make sure the files you are watching exist and are readable.\n");
	}
}

.ExecWatchCallback <- function( path ){
	cat(paste("Executing code on file change:", path, "\n" ));
	do.call(.WatchedFiles[[path]], list());
}

#--------------------------------------------------------
# watch file, execute code on change
#--------------------------------------------------------
WatchFile <- function( path, FUN = BERT$ReloadStartup ){
	.WatchedFiles[[path]] = FUN;
	.RestartWatch();
} 

#--------------------------------------------------------
# stop watching file (by path)
#--------------------------------------------------------
UnwatchFile <- function( path ){
	rm( list=path, envir=.WatchedFiles );
	.RestartWatch();
}

#--------------------------------------------------------
# remove all watches
#--------------------------------------------------------
ClearWatches <- function(){
	rm( list=ls(.WatchedFiles), envir=.WatchedFiles );
	.RestartWatch();
}

#--------------------------------------------------------
# list watches - useful if something unexpected is happening
#--------------------------------------------------------
ListWatches <- function(){
	ls(.WatchedFiles);
}

#--------------------------------------------------------
# Excel callback function. Be careful with this unless 
# you know what you are doing.
#--------------------------------------------------------
.Excel<- function( command, arguments = list() ){ .Call(.CALLBACK, .EXCEL, command, arguments, PACKAGE=.MODULE ); };

#========================================================
# functions for using the Excel COM interface 
# (experimental)
#========================================================

#--------------------------------------------------------
# create a wrapper for a dispatch pointer -- this will
# have functions in the environment
#--------------------------------------------------------
.WrapDispatch <- function( class.name = NULL ){
	
	obj <- new.env();
	if( is.null( class.name )){ class(obj) <- "IDispatch"; }
	else { class(obj) <- c( class.name, "IDispatch" ) };
	return(obj);

}

#--------------------------------------------------------
# callback function for handling com calls 
#--------------------------------------------------------	
.DefineCOMFunc <- function( func.name, func.type, func.args, target.env ){

	if( missing( func.args ) || length(func.args) == 0 ){
		target.env[[func.name]] <- function(...){ 
			.Call(.COM_CALLBACK, func.name, func.type, target.env$.p, list(...), PACKAGE=.MODULE );
		}
	}
	else {
		target.env[[func.name]] <- function(){ 
			.Call(.COM_CALLBACK, func.name, func.type, target.env$.p, c(as.list(environment())), PACKAGE=.MODULE );
		}
		aexp <- paste( "alist(", paste( sapply( func.args, function(x){ paste( x, "=", sep="" )}), collapse=", " ), ")" );
		formals(target.env[[func.name]]) <- eval(parse(text=aexp));
	}
}

#========================================================
# autocomplete
#========================================================

.Autocomplete <- function(...){
	
	ac <- utils:::.win32consoleCompletion(...);
	if( length( utils:::.CompletionEnv$comps) > 0 ){
		ac$comps <- paste( utils:::.CompletionEnv$comps, collapse='\n' );
	}

	ac$function.signature <- ifelse( is.null( utils:::.CompletionEnv$function.signature ), "", utils:::.CompletionEnv$function.signature );
	ac$token <- ifelse( is.null( utils:::.CompletionEnv$token ), "", utils:::.CompletionEnv$token );
	ac$fguess <- ifelse( is.null( utils:::.CompletionEnv$fguess ), "", utils:::.CompletionEnv$fguess );
	ac$start <- utils:::.CompletionEnv$start;
	ac$end <- utils:::.CompletionEnv$end;
	# ac$file.name <- utils:::.CompletionEnv$fileName;
	ac$in.quotes <- utils:::.CompletionEnv$in.quotes;

	ac;
}

#--------------------------------------------------------
# this is a monkeypatch for the existing R autocomplete 
# functionality. we are making two changes: (1) for 
# functions, store the signagure for use as a call tip.  
# (2) for functions within environments, resolve and get 
# parameters.
#
# update: now delegating file completion to C (probably
# more to come).
#--------------------------------------------------------
rc.options( custom.completer= function (.CompletionEnv) 
{
	.fqFunc <- function (line, cursor=-1) 
	{
		localBreakRE <- "[^\\.\\w\\$\\@\\:]";

		if( cursor == -1 ){ cursor = nchar(line); }

	    parens <- sapply(c("(", ")"), function(s) gregexpr(s, substr(line, 
		1L, cursor), fixed = TRUE)[[1L]], simplify = FALSE)
	    parens <- lapply(parens, function(x) x[x > 0])
	       
	    
	    temp <- data.frame(i = c(parens[["("]], parens[[")"]]), c = rep(c(1, 
		-1), lengths(parens)))
	    if (nrow(temp) == 0) 
		return(character())
		
	    temp <- temp[order(-temp$i), , drop = FALSE]
	    wp <- which(cumsum(temp$c) > 0)

	    if (length(wp)) {
		index <- temp$i[wp[1L]]
		prefix <- substr(line, 1L, index - 1L)
		suffix <- substr(line, index + 1L, cursor + 1L)
		
		if ((length(grep("=", suffix, fixed = TRUE)) == 0L) && 
		    (length(grep(",", suffix, fixed = TRUE)) == 0L)) 
		    utils:::setIsFirstArg(TRUE)
		if ((length(grep("=", suffix, fixed = TRUE))) && (length(grep(",", 
		    substr(suffix, utils:::tail.default(gregexpr("=", suffix, 
			fixed = TRUE)[[1L]], 1L), 1000000L), fixed = TRUE)) == 
		    0L)) {
		    return(character())
		}
		else {
		    possible <- suppressWarnings(strsplit(prefix, localBreakRE, 
			perl = TRUE))[[1L]]
		    possible <- possible[nzchar(possible)]
		    if (length(possible)) 
			return(utils:::tail.default(possible, 1))
		    else return(character())
		}
	    }
	    else {
		return(character())
	    }
	}

	.fqFunctionArgs <- function (fun, text, S3methods = utils:::.CompletionEnv$settings[["S3"]], 
	    S4methods = FALSE, add.args = rc.getOption("funarg.suffix")) 
	{
	
		.resolveObject <- function( name ){

			p <- environment();
			n <- unlist( strsplit( name, "[^\\w\\.]", F, T ));
			 while( length( n ) > 1 ){
				if( n == "" || !exists( n[1], where=p )) return( NULL );
				p <- get( n[1], envir=p );
				n <- n[-1];
			}
			if( n == "" || !exists( n[1], where=p )) return( NULL );
			list( name=n[1], fun=get( n[1], envir=p ));
		}
	
		.function.signature <- function(fun){
			x <- capture.output( args(fun));
			paste(trimws(x[-length(x)]), collapse=" ");
		}
	
		.fqArgNames <- function (fname, use.arg.db = utils:::.CompletionEnv$settings[["argdb"]]) 
		{
			funlist <- .resolveObject( fname );
			fun <- funlist$fun;
			if( !is.null(fun) && is.function(fun )) { 
				env <- utils:::.CompletionEnv;
				env$function.signature <- sub( '^function ', paste0( funlist$name, ' ' ), .function.signature(fun));
				return(names( formals( fun ))); 
			}
			return( character());
		};

		if (length(fun) < 1L || any(fun == "")) 
			return(character())
		    specialFunArgs <- utils:::specialFunctionArgs(fun, text)
		if (S3methods && exists(fun, mode = "function")) 
			fun <- c(fun, tryCatch(methods(fun), warning = function(w) {
			}, error = function(e) {
			}))
		if (S4methods) 
			warning("cannot handle S4 methods yet")
		allArgs <- unique(unlist(lapply(fun, .fqArgNames)))
		ans <- utils:::findMatches(sprintf("^%s", utils:::makeRegexpSafe(text)), 
			allArgs)
		if (length(ans) && !is.null(add.args)) 
			ans <- sprintf("%s%s", ans, add.args)
		c(specialFunArgs, ans)
	}

	.CompletionEnv[["function.signature"]] <- "";
	.CompletionEnv[["in.quotes"]] <- F;

	    text <- .CompletionEnv[["token"]]
	    if (utils:::isInsideQuotes()) {
		{
		    .CompletionEnv[["comps"]] <- character()
			.CompletionEnv[["in.quotes"]] <- T;
		    utils:::.setFileComp(TRUE)
		}
	    }
	    else {
		utils:::.setFileComp(FALSE)
		utils:::setIsFirstArg(FALSE)
		guessedFunction <- if (.CompletionEnv$settings[["args"]]) 
		    .fqFunc(.CompletionEnv[["linebuffer"]], .CompletionEnv[["start"]])
		else ""
		
		.CompletionEnv[["fguess"]] <- guessedFunction
		fargComps <- .fqFunctionArgs(guessedFunction, text)
		
		if (utils:::getIsFirstArg() && length(guessedFunction) && guessedFunction %in% 
		    c("library", "require", "data")) {
		    .CompletionEnv[["comps"]] <- fargComps
		    return()
		}
		lastArithOp <- utils:::tail.default(gregexpr("[\"'^/*+-]", text)[[1L]], 
		    1)
		if (haveArithOp <- (lastArithOp > 0)) {
		    prefix <- substr(text, 1L, lastArithOp)
		    text <- substr(text, lastArithOp + 1L, 1000000L)
		}
		spl <- utils:::specialOpLocs(text)
		comps <- if (length(spl)) 
		    utils:::specialCompletions(text, spl)
		else {
		    appendFunctionSuffix <- !any(guessedFunction %in% 
			c("help", "args", "formals", "example", "do.call", 
			  "environment", "page", "apply", "sapply", "lapply", 
			  "tapply", "mapply", "methods", "fix", "edit"))
		    utils:::normalCompletions(text, check.mode = appendFunctionSuffix)
		}
		if (haveArithOp && length(comps)) {
		    comps <- paste0(prefix, comps)
		}
		comps <- c(fargComps, comps)
		.CompletionEnv[["comps"]] <- comps
	    }
});


}); # end with(BERT)


#========================================================
# utility methods and types
#========================================================

BERT$Util <- new.env();
with( BERT$Util, {

#--------------------------------------------------------
# xlReference: s4 class type representing an Excel 
# cell reference.  
#--------------------------------------------------------
setClass( "xlReference", 
	slots = c( R1 = "numeric", C1 = "numeric", R2 = "numeric", C2 = "numeric", SheetID = "numeric" ),
	prototype = list( R1 = 0, C1 = 0, R2 = 0, C2 = 0, SheetID = c(0,0))
	);

suppressMessages(setMethod( "nrow", "xlReference", function(x){ 
	if( x@R2 >= x@R1 ){ return( x@R2-x@R1+1 ); }
	else{ return(1); }
}, where = BERT$Util));

suppressMessages(setMethod( "ncol", "xlReference", function(x){ 
	if( x@C2 >= x@C1 ){ return( x@C2-x@C1+1 ); }
	else{ return(1); }
}, where = BERT$Util));

# length isn't really appropriate for this object
#setMethod( "length", "xlReference", function(x){ return(nrow(x) * ncol(x)); });

setMethod( "show", "xlReference", function(object){
	cat( "Excel Reference ", "R", object@R1, "C", object@C1, sep="" );
	if( object@R2 > object@R1 || object@C2 > object@C1 )
	{
		cat( ":", "R", object@R2, "C", object@C2, sep="" );
	}
	if( object@SheetID[1] != 0 || object@SheetID[2] != 0 )
	{
		cat( " SheetID ", object@SheetID[1], ".", object@SheetID[2], sep="");
	}
	cat( "\n" );
});

#--------------------------------------------------------
# overloading history for the console
#--------------------------------------------------------
history <- function( max.show=25, reverse=FALSE, pattern="" ){
	.Call( BERT$.CALLBACK, BERT$.HISTORY, list(max.show, reverse, pattern), 0, PACKAGE=BERT$.MODULE );
}

#--------------------------------------------------------
# history is now returned as a list; but we want to 
# do some special formatting, so use a generic.
#--------------------------------------------------------
print.history.list <- function(h){

	len <- length(h);
	pattern <- paste( "  %s\n", sep="" );

	cat( "\n" );
	for( i in 1:len ){ cat( sprintf( pattern, h[i] )); }
	cat( "\n" );

}


#--------------------------------------------------------
# overload quit method or it will stop the excel process
#--------------------------------------------------------

quit <- function(){ BERT$CloseConsole() }
q <- quit;

}); # end with(BERT$Util)

suppressMessages(attach( BERT$Util ));



