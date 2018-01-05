#
# Copyright (c) 2017 Structured Data LLC
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

#
# we need a module for the graphics device because R wants to free() 
# it and we can't agree on an allocator (not sure why not).  since we 
# have a module, though, this will probably become the default location
# for R functions.  nice to have a real namespace as well.
#

#' @useDynLib BERTModule
NA

#==============================================================================
#
# scratch
#
#==============================================================================

data.env <- new.env();

#==============================================================================
#
# utility functions
#
#==============================================================================

#' Convert a list of lists into a data frame
#'
#' Spreadsheet functions that accept a range as an argument and range values 
#' returned by the COM/scripting API may be returned as list-of-list 
#' structures.  BERT does that to preserve type information; R vectors can't 
#' contain mixed types.  This function will convert a list-of-lists structure
#' into a data frame, optionally with column headers.
#'
#' @export range.to.data.frame
range.to.data.frame <- function( rng, headers=F ){

	# remove headers from data if necessary
	data <- if(headers) rng[-1,] else rng;

	# format data
  if( is.null( ncol(data))){
    # special case 
    df <- as.data.frame( unlist( data ));
  }
  else {
  	df <- as.data.frame( lapply( split( data, col(data)), unlist ));
  }

	# add headers if available
	if( headers ){ colnames(df) <- rng[1,]; }

	# done
	df;  

}

#' pad an excel array with empty strings
#'
#' if you enter an R function in an Excel array, but the function result
#' is smaller than the Excel range, the range will be filled with #N/A errors.
#' this is the correct behavior, but it can be unattractive. this function
#' will check the size of the output range, and if the data is smaller, will
#' fill the range with empty strings ("") that render as empty cells in Excel.
#'
#' @export pad.excel.array
pad.excel.array <- function(rslt){

  # scalar
  if( length(rslt) == 1 ){ return(rslt); }

  # xlfCaller
  ref <- BERT$.Excel(89); 
  n.rows <- nrow(ref);
  n.cols <- ncol(ref);

  # not square, need to think about proper behavior
  if(is.null(nrow(rslt))){
    return(matrix(rslt, nrow=n.rows, ncol=n.cols));
  }

  # create matrix of lists, default empty string
  mat <- matrix(list(""), nrow=n.rows, ncol=n.cols);

  # fill
  for( i in 1:(min(n.rows, nrow(rslt)))){
    for( j in 1:(min(n.cols, ncol(rslt)))){
      mat[i,j] <- list(rslt[i,j]);
    }
  }

  mat; # rslt;
}

#==============================================================================
#
# graphics device 
#
#==============================================================================

#' Create a BERT graphics device.
#'
#' \code{BERT.graphics.device} creates a graphics device that renders to 
#' a named Shape in an Excel workbook.  
#'
#' If this is called from an Excel spreadsheet cell, set \code{cell=TRUE} 
#' and it will use a name that's generated from the cell address, which 
#' should be unique in the workbook.  (That won't survive sheet renaming, 
#' but in that case just toss the old one and a new one will be generated 
#' on the next paint).
#'
#' Size, background and pointsize arguments are ignored if the target 
#' named shape already exists.  The values for these arguments are 
#' scaled based on reported screen DPI to give reasonable values on 
#' normal and high-DPI displays (to prevent this behavior, set the scale 
#' parameter to 1).
#'
#' @export 
BERT.graphics.device <- function( name="BERT-default", bgcolor="white", width=400, height=400, pointsize=14, scale=Sys.getenv("BERTGraphicsScale"), cell=F ){

  scale <- as.numeric(scale);
  if(is.na(scale) | is.null(scale)){ scale = 1; }

  width = round( width * scale ); 
  height = round( height * scale ); 
  pointsize = round( pointsize * scale ); 

  if( cell ){
  	ref <- BERT$.Excel(89); # xlfCaller
	  sheetnm <- BERT$.Excel(0x4005, list(ref)); # xlSheetNm
  	name = paste0( gsub( "\\[.*?\\]", "", sheetnm ), " R", ref@R1, " C", ref@C1 );
  }

	x <- dev.list();
	if((length(x) > 0) & (name %in% names(x))){ dev.set( x[[name]]) }
	else {
    .Call( "create_device", name, bgcolor, width, height, pointsize, PACKAGE='BERTModule' );
	}
}


#==============================================================================
#
# progress bars
#
#==============================================================================

with( data.env, {
  progress.bar.list <- list();
  progress.bar.key <- 1;
});

js.client.progress.bar <- function( min=0, max=1, initial=min, ... ){

  key <- data.env$progress.bar.key;
  struct <- list( key=key, min=min, max=max, initial=initial, value=initial, ... );
  handle <- list( key=key );
  class(handle) <- "js.client.progress.bar";

  pblist <- data.env$progress.bar.list;
  pblist[[toString(key)]] <- struct; 

  assign( "progress.bar.list", pblist, envir=data.env );  
  assign( "progress.bar.key", key + 1, envir=data.env );  

  invisible(.Call("progress_bar", struct, PACKAGE="BERTModule" ));
  return(handle);
}

js.client.get.progress.bar <- function( pb ){
  struct <- data.env$progress.bar.list[[toString(pb$key)]];
  return( struct$value );	
}

js.client.set.progress.bar <- function( pb, value, title=NULL, label=NULL ){
  struct <- data.env$progress.bar.list[[toString(pb$key)]];
  struct$value <- value;
  if( !is.null(label)){ struct$label = label }

  pblist <- data.env$progress.bar.list;
  pblist[[toString(pb$key)]] <- struct;
  assign( "progress.bar.list", pblist, envir=data.env );
 
  invisible(.Call("progress_bar", struct, PACKAGE="BERTModule" ));
  return( struct$value );	
}

#' Generic method for BERT shell progress bar
#'
#' @export 
close.js.client.progress.bar <- function( pb ){
  struct <- data.env$progress.bar.list[[toString(pb$key)]];
  struct$closed <- T;
  invisible(.Call("progress_bar", struct, PACKAGE="BERTModule" ));

  pblist <- data.env$progress.bar.list;
  pblist[[toString(pb$key)]] <- NULL;
  assign( "progress.bar.list", pblist, envir=data.env );
}


#==============================================================================
#
# history (just calls into BERT; but we have a special print generic)
#
#==============================================================================

#' Print console history
#'
#' @export
print.history.list <- function(h){
	len <- length(h);
	pattern <- paste( "  %s\n", sep="" );
	cat( "\n" );
	for( i in 1:len ){ cat( sprintf( pattern, h[i] )); }
	cat( "\n" );
}

#' History implementation for the BERT console
#' 
history <- function( max.show=25, reverse=FALSE, pattern="" ){
  .Call( "history", list(max.show, reverse, pattern), PACKAGE="BERTModule" );
}

#==============================================================================
#
# dev
#
#==============================================================================
.Callback <- function( command, data ){
  .Call("Callback", command, data, PACKAGE="BERTModule");
}

#==============================================================================
#
# functions for using the Excel COM interface 
#
#==============================================================================

#' create a wrapper for a dispatch pointer
.WrapDispatch <- function( class.name = NULL ){
	
	obj <- new.env();
	if( is.null( class.name )){ class(obj) <- "IDispatch"; }
	else { class(obj) <- c( class.name, "IDispatch" ) };
	return(obj);

}

#' create a wrapper for a dispatch pointer (2)
.WrapDispatch2 <- function( key, class.name = NULL ){
	
	obj <- new.env();
	if( is.null( class.name )){ class(obj) <- "IDispatch"; }
	else { class(obj) <- c( class.name, "IDispatch" ) };

  pointer <- .Call("InstallPointer", key, PACKAGE="BERTModule");
  assign( ".p", pointer, env=obj );

	return(obj);

}

.WrapDispatch3 <- function( pointer, class.name = NULL ){
	env <- new.env();
	if( is.null( class.name )){ class(env) <- "IDispatch"; }
	else { class(env) <- c( class.name, "IDispatch" ) };
  assign( ".p", pointer, env=env );
	return(env);
}

#' callback function for handling com calls 
.DefineCOMFunc <- function( func.name, base.name, func.type, func.index, func.args, target.env ){

	if( missing( func.args ) || length(func.args) == 0 ){
		target.env[[func.name]] <- function(...){ 
      .Call("COMCallback", base.name, func.type, func.index, target.env$.p, list(...), PACKAGE="BERTModule" );
		}
	}
	else {
		target.env[[func.name]] <- function(){ 
			.Call("COMCallback", base.name, func.type, func.index, target.env$.p, c(as.list(environment())), PACKAGE="BERTModule" );
		}
		aexp <- paste( "alist(", paste( sapply( func.args, function(x){ paste( x, "=", sep="" )}), collapse=", " ), ")" );
		formals(target.env[[func.name]]) <- eval(parse(text=aexp));
	}
}


#==============================================================================
#
# autocomplete
#
#==============================================================================

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

#
# this is a monkeypatch for the existing R autocomplete # functionality. we are making two 
# changes: (1) for functions, store the signagure for use as a call tip. (2) for functions 
# within environments, resolve and get parameters.
#
# update: now delegating file completion to C (probably more to come).
#
.CustomCompleter <- function(.CompletionEnv){

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
			n <- unlist( strsplit( name, "[^\\w\\.,]", F, T ));
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
};

#==============================================================================
#
# exported generics for xlReference
#
#==============================================================================

#' @export nrow
nrow.xlReference <- function(x){ 
  if( x@R2 >= x@R1 ){ return( x@R2-x@R1+1 ); }
  else{ return(1); }
}

#' @export ncol
ncol.xlReference <- function(x){ 
  if( x@C2 >= x@C1 ){ return( x@C2-x@C1+1 ); }
  else{ return(1); }
}

#==============================================================================
#
# .onLoad
#
#==============================================================================

.onLoad <- function(libname, pkgname){

  #-----------------------------------------------------------------------------
  # replace win progress bar with an inline progress bar.  
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

#  override.binding( "winProgressBar", js.client.progress.bar, "utils");
#  override.binding( "setWinProgressBar", js.client.set.progress.bar, "utils");
#  override.binding( "getWinProgressBar", js.client.get.progress.bar, "utils");

#  override.binding( "txtProgressBar", js.client.progress.bar, "utils");
#  override.binding( "setTxtProgressBar", js.client.set.progress.bar, "utils");
#  override.binding( "getTxtProgressBar", js.client.get.progress.bar, "utils");

  #-----------------------------------------------------------------------------
  # create a "js" download method, and set that as default
  #-----------------------------------------------------------------------------

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
        invisible(.Call("download", as.list(environment()), PACKAGE="BERTModule" ));
      }
      else {
        do.call( download.file.original, as.list(environment()), envir=parent.env(environment()));
      }
    }, "utils", T );

  options( download.file.method="js" );

  #-----------------------------------------------------------------------------
  # overload history for the console
  #--------------------------------------------------------

  override.binding( "history", history, "utils");

  #-----------------------------------------------------------------------------
  # override quit so it doesn't kill the excel process
  #-----------------------------------------------------------------------------

  quit <- function(){ invisible(.Call("close_console", PACKAGE="BERTModule" )); }
  override.binding( "quit", quit, "base");
  override.binding( "q", quit, "base");

  #-----------------------------------------------------------------------------
  # autocomplete
  #-----------------------------------------------------------------------------

  rc.options( custom.completer=.CustomCompleter );

  #-----------------------------------------------------------------------------
  # xlReference: s4 class type representing an Excel cell reference.  this 
  # was more useful before we added the COM interface, but it still might 
  # be handy.
  #-----------------------------------------------------------------------------

  setClass( "xlReference", 
    slots = c( R1 = "numeric", C1 = "numeric", R2 = "numeric", C2 = "numeric", SheetID = "numeric" ),
    prototype = list( R1 = 0, C1 = 0, R2 = 0, C2 = 0, SheetID = c(0,0))
  );

  # why can't nrow and ncol be defined the same way as show?
  # it just doesn't work.  in any event these need to be exported.

  suppressMessages(setMethod( "nrow", "xlReference", nrow.xlReference ));
  suppressMessages(setMethod( "ncol", "xlReference", ncol.xlReference ));

  # this one seems to work regardless

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

}


