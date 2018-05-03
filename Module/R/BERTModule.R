#
# Copyright (c) 2017-2018 Structured Data, LLC
# 
# This file is part of BERT.
#
# BERT is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# BERT is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with BERT.  If not, see <http://www.gnu.org/licenses/>.
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
range.to.data.frame <- function(rng, headers = FALSE, stringsAsFactors = default.stringsAsFactors()) {

  # fix for issue #121
  rng <- replace(rng, rng=="NULL", NA);

  # remove headers from data if necessary
  data <- if (headers) rng[-1, ] else rng;

  # format data
  if (is.null(ncol(data))) {

    # special case
    df <- as.data.frame(unlist(data), stringsAsFactors = stringsAsFactors);
  }
  else {
    df <- as.data.frame(lapply(split(data, col(data)), unlist), stringsAsFactors = stringsAsFactors);
  }

  # add headers if available
  if (headers) { colnames(df) <- rng[1,]; }

  df
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
BERT.graphics.device <- function( name="BERT-default", bgcolor="white", width=400, height=400, pointsize=12, scale=Sys.getenv("BERTGraphicsScale"), cell=F ){

  scale <- as.numeric(scale);
  if(is.na(scale) | is.null(scale)){ scale = 1; }

  width = round( width * scale ); 
  height = round( height * scale ); 
  pointsize = round( pointsize * scale ); 

  if( cell ){
  	ref <- BERT$.Excel(89); # xlfCaller
	  sheetnm <- BERT$.Excel(0x4005, ref); # xlSheetNm
  	name = paste0( gsub( "\\[.*?\\]", "", sheetnm ), " R", ref@R1, " C", ref@C1 );
  }

	x <- dev.list();
	if((length(x) > 0) & (name %in% names(x))){ dev.set( x[[name]]) }
	else {
    .Call( "spreadsheet_device", name, bgcolor, width, height, pointsize, PACKAGE='BERTModule' );
	}
}

#' Create a console graphics device.
#'
#' \code{BERT.console.graphics.device} creates a graphics device that 
#' renders to the shell. type is either "svg" or "png".
#'
#' @export 
BERT.console.graphics.device <- function( bgcolor="white", width=500, height=350, pointsize=14, type="png"){

  # default to png, enforce
  if(type != "svg"){ type = "png"; }

  name <- paste0("BERT Console (", type, ")");
	x <- dev.list();
	if((length(x) > 0) & (name %in% names(x))){ dev.off( x[[name]]); }

  .Call( "console_device", bgcolor, width, height, pointsize, type, PACKAGE='BERTModule' );

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
# history calls back into console, which maintains history. we return this 
# as a list, and we have a special print generic
#
#==============================================================================

#' Print console history
#'
#' @export
print.ordered.history <- function(h){
	cat("", sapply(h$indexes, function(index){
    paste(formatC(index, width=6), " ", h$lines[index]);
  }), "", sep="\n");
}

#' History implementation for the BERT console
#' 
history <- function( max.show=25, reverse=FALSE, pattern, ... ){
  lines <- .Call( "history", list(), PACKAGE="BERTModule" );
  if(missing(pattern)){ indexes = 1:(length(lines)); }
  else { indexes = grep(pattern, lines, ...); }
  indexes = tail(indexes, max.show);
  if(reverse) indexes = rev(indexes);
  result = list(indexes=indexes, lines=lines);
  class(result) <- "ordered.history";
  result;
}

#==============================================================================
#
# dev
#
#==============================================================================
#.Callback <- function( command, data ){
#  .Call("Callback", command, data, PACKAGE="BERTModule");
#}

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

#  download.file.original <- get( "download.file", envir=as.environment( "package:utils" ));
#  override.binding( "download.file",  
#    function (url, destfile, method, quiet = FALSE, mode = "w", cacheOK = TRUE, 
#        extra = getOption("download.file.extra")) 
#    {
#      method <- if (missing(method)) 
#        getOption("download.file.method", default = "auto")
#      else match.arg(method, c("auto", "internal", "wininet", "libcurl", 
#        "wget", "curl", "lynx", "js"))
#      if( method == "js" ){
#        invisible(.Call("download", as.list(environment()), PACKAGE="BERTModule" ));
#      }
#      else {
#        do.call( download.file.original, as.list(environment()), envir=parent.env(environment()));
#      }
#    }, "utils", T );
#
#  options( download.file.method="js" );

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
  # xlReference: s4 class type representing an Excel cell reference.  this 
  # was more useful before we added the COM interface, but it  might 
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


