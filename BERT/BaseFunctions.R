
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

.PROGRESSBAR <- 2000;
.DOWNLOAD <- 2001;

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
# reload startup files
#--------------------------------------------------------
ReloadStartup <- function(){ .Call(.CALLBACK, .RELOAD, 0, PACKAGE=.MODULE ); };

#--------------------------------------------------------
# reload functions into excel.  this assumes that the 
# functions have already been sourced() into R.
#--------------------------------------------------------
RemapFunctions <- function(){ .Call( .CALLBACK, .REMAP_FUNCTIONS, 0, PACKAGE=.MODULE ); };

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
	require( pkg, character.only=T );
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

	# we watch the functions dir by default.  you can overload behavior by
	# watching the folder with a specific function.

	path = gsub( "\\\\+$", "", gsub( "/", "\\\\", tolower(normalizePath(BERT$STARTUP_FOLDER))));
	if( !exists( path, envir=.WatchedFiles )) .WatchedFiles[[path]] = NULL;

	rslt <- .Call( BERT$.CALLBACK, BERT$.WATCHFILES, ls(.WatchedFiles), 0, PACKAGE=BERT$.MODULE );
	if( !rslt ){
		cat( "File watch failed.  Make sure the files you are watching exist and are readable.\n");
	}
}
.RestartWatch();

.ExecWatchCallback <- function( path ){

	path = gsub( "\\\\+$", "", gsub( "/", "\\\\", tolower(normalizePath(path)) ))

	# it's possible that both the directory and the specific file are 
	# being watched.  we want to support that pattern.  check the directory
	# first and execute (call with the path to the changed file)

	dir = gsub( "\\\\+$", "", gsub( "/", "\\\\", dirname(path)))
	if( exists( dir, envir=.WatchedFiles )){
		FUN = .WatchedFiles[[dir]];
		if( is.null(FUN)){
			FUN = function(a){
				if( grepl( "\\.(?:rscript|r|rsrc)$", a, ignore.case=T )){
					source(a, chdir=T);
					BERT$RemapFunctions();
				}
				else {
					cat("skipping file (invalid extension)\n");
				}
			}
		}
		cat(paste("Executing code on file change:", path, "\n" ));
		do.call(FUN, list(path));
	}

	# now look for the specific file and execute that function.
	# for backwards-compatibility purposes there are no arguments.

	if( exists( path, envir=.WatchedFiles )){
		FUN = .WatchedFiles[[path]];
		if( is.null(FUN)){
			FUN = function(a=path){
				if( grepl( "\\.(?:rscript|r|rsrc)$", a, ignore.case=T )){
					source(a, chdir=T);
					BERT$RemapFunctions();
				}
				else {
					cat("skipping file (invalid extension)\n");
				}
			}
		}
		cat(paste("Executing code on file change:", path, "\n" ));
		do.call(FUN, list());
	}

}

#--------------------------------------------------------
# watch file, execute code on change
#--------------------------------------------------------
WatchFile <- function( path, FUN=NULL, apply.now=F ){
	path = gsub( "\\\\+$", "", gsub( "/", "\\\\", tolower(normalizePath(path)) ));
	.WatchedFiles[[path]] = FUN;
	.RestartWatch();
} 

#--------------------------------------------------------
# stop watching file (by path)
#--------------------------------------------------------
UnwatchFile <- function( path ){
	path = gsub( "\\\\+$", "", gsub( "/", "\\\\", tolower(normalizePath(path)) ))
	rm( list=path, envir=.WatchedFiles );
	.RestartWatch();
}

#--------------------------------------------------------
# remove all watches (except default)
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

#========================================================
# progress bars
#========================================================

	progress.bars <- new.env();
	with( progress.bars, {

		progress.bar.list <- list();
		progress.bar.key <- 1;

		js.client.progress.bar <- function( min=0, max=1, initial=min, ... ){

			key <- progress.bar.key;
			struct <- list( key=key, min=min, max=max, initial=initial, value=initial, ... );
			handle <- list( key=key );
			class(handle) <- "js.client.progress.bar";
			progress.bar.list[[toString(key)]] <<- struct;
			# .js.client.callback( "progress.bar", struct );
		
			invisible(.Call(.CALLBACK, .PROGRESSBAR, struct, PACKAGE=.MODULE ));

			# increment key	for next call
			progress.bar.key <<- progress.bar.key + 1;	
	
			return(handle);
		}

		js.client.get.progress.bar <- function( pb ){
			struct <- progress.bar.list[[toString(pb$key)]];
			return( struct$value );	
		}

		js.client.set.progress.bar <- function( pb, value, title=NULL, label=NULL ){
			struct <- progress.bar.list[[toString(pb$key)]];
			struct$value <- value;
			if( !is.null(label)){ struct$label = label }
			progress.bar.list[[toString(pb$key)]] <<- struct;
			# .js.client.callback( "progress.bar", struct );
			invisible(.Call(.CALLBACK, .PROGRESSBAR, struct, PACKAGE=.MODULE ));
			return( struct$value );	
		}

		close.js.client.progress.bar <- function( pb ){
			struct <- progress.bar.list[[toString(pb$key)]];
			struct$closed <- T;
			# .js.client.callback( "progress.bar", struct );
			invisible(.Call(.CALLBACK, .PROGRESSBAR, struct, PACKAGE=.MODULE ));
			progress.bar.list[[toString(pb$key)]] <<- NULL;
		}


		(function(){

			#-----------------------------------------------------------------------------
			# replace win progress bar with an inline progress bar.  there's a slight
			# difference in that these functions might not exist on linux.
			#-----------------------------------------------------------------------------

			override.binding <- function( name, func, ns, assign.in.namespace=T ){
				if( exists( name ) ){ 
					package <- paste0( "package:", ns );
					unlockBinding( name, as.environment(package)); 
					assign( name, func, as.environment(package));
					if( assign.in.namespace ){
						ns <- asNamespace( ns );
						if (bindingIsLocked(name, ns)) {
							unlockBinding(name, ns)
							assign(name, func, envir = ns, inherits = FALSE)
							w <- options("warn")
							on.exit(options(w))
							options(warn = -1)
							lockBinding(name, ns)
						}
						else assign(name, func, envir = ns, inherits = FALSE);
					}
					lockBinding( name, as.environment(package)); 
				}
			}

			override.binding( "winProgressBar", js.client.progress.bar, "utils");
			override.binding( "setWinProgressBar", js.client.set.progress.bar, "utils");
			override.binding( "getWinProgressBar", js.client.get.progress.bar, "utils");

			override.binding( "txtProgressBar", js.client.progress.bar, "utils");
			override.binding( "setTxtProgressBar", js.client.set.progress.bar, "utils");
			override.binding( "getTxtProgressBar", js.client.get.progress.bar, "utils");

			download.file.original <- get( "download.file", envir=as.environment( "package:utils" ));
			override.binding( "download.file",  
				function (url, destfile, method, quiet = FALSE, mode = "w", cacheOK = TRUE, 
    				extra = getOption("download.file.extra")) 
				{
					method <- if (missing(method)) 
						getOption("download.file.method", default = "auto")
					else match.arg(method, c("auto", "internal", "wininet", "libcurl", 
						"wget", "curl", "lynx", "js"))
					if( method == "js" ){
						# jsClientLib:::.js.client.callback.sync( list( arguments=as.list(environment()), command="download" ));
						invisible(.Call(.CALLBACK, .DOWNLOAD, as.list(environment()), PACKAGE=.MODULE ));
					}
					else {
						do.call( download.file.original, as.list(environment()), envir=parent.env(environment()));
					}
				}, "utils", T );

			options( download.file.method="js" );

		})();

	}); # end with (progress.bars)

}); # end with(BERT)

attach( BERT$progress.bars );

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

#--------------------------------------------------------
# convert an Excel range to a data frame, optionally 
# with headers.
#--------------------------------------------------------

range.to.data.frame <- function( rng, headers=F ){

	# remove headers from data if necessary
	data <- if(headers) rng[-1,] else rng;

	# format data
	df <- as.data.frame( lapply( split( data, col(data)), unlist ));

	# add headers if available
	if( headers ){ colnames(df) <- rng[1,]; }

	# done
	df;  

}

}); # end with(BERT$Util)

suppressMessages(attach( BERT$Util ));



