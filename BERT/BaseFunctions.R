
with( BERT, {

#========================================================
# constants
#========================================================

.EXCEL <- 1;
.WATCHFILES <- 1020;
.CLEAR <- 1021;
.RELOAD <- 1022;
.CLOSECONSOLE <- 1023;

.CALLBACK <- "BERT_Callback";

#========================================================
# API for the file watch utility
#========================================================

.WatchedFiles <- new.env();

.RestartWatch <- function(){
	.Call( .CALLBACK, .WATCHFILES, ls(.WatchedFiles), 0 );
}

.ExecWatchCallback <- function( path ){
	cat(paste("Executing code on file change:", path, "\n" ));
	do.call(.WatchedFiles[[path]], list());
}

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

#--------------------------------------------------------
# watch file, execute code on change
#--------------------------------------------------------
WatchFile <- function( path, code = ReloadStartup ){
	.WatchedFiles[[path]] = code;
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
# Excel callback function. Be careful with this unless 
# you know what you are doing.
#--------------------------------------------------------
.Excel<- function( command, arguments = list() ){ .Call(.CALLBACK, .EXCEL, command, arguments, PACKAGE=.MODULE ); };

#========================================================
# functions - for internal use
#========================================================

WordList <- function(){ 
	wl <- vector();
	for( i in search()){ wl <- c( wl, ls(i, all.names=1)); }
	wl;
}

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
# overload quit method or it will stop the excel process
#--------------------------------------------------------

quit <- function(){ BERT$CloseConsole() }
q <- quit;

}); # end with(BERT$Util)

suppressMessages(attach( BERT$Util ));



