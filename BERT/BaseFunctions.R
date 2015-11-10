
with( BERT, {

#========================================================
# constants
#========================================================

.EXCEL <- 1;

.ADD_USER_BUTTON <- 100;
.CLEAR_USER_BUTTONS <- 101;

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

.UserButtonCallbacks <- c();
.UserButtonCallback <- function(index){
	return(.UserButtonCallbacks[[index+1]]());
}

#--------------------------------------------------------
# add a user button.  these are added to the ribbon,
# there's a max of 6.  callback is an R function.
#--------------------------------------------------------
AddUserButton <- function( label, FUN, imageMso = NULL ){
	.Call(.CALLBACK, .ADD_USER_BUTTON, c( label, "BERT$.UserButtonCallback", imageMso ), 0, PACKAGE=.MODULE);
	.UserButtonCallbacks <<- c(.UserButtonCallbacks, FUN);
}

#--------------------------------------------------------
# remove user buttons
#--------------------------------------------------------
ClearUserButtons <- function(){
	.UserButtonCallbacks <<- c();
	.Call(.CALLBACK, .CLEAR_USER_BUTTONS, 0, 0, PACKAGE=.MODULE);
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
WatchFile <- function( path, code = BERT$ReloadStartup ){
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
# list watches - useful if something unexpected is happening
#--------------------------------------------------------
ListWatches <- function(){
	ls(.WatchedFiles);
}

#--------------------------------------------------------
# overload quit method or it will stop the excel process
#--------------------------------------------------------

quit <- function(){ BERT$CloseConsole() }
q <- quit;

}); # end with(BERT$Util)

suppressMessages(attach( BERT$Util ));



