
with( BERT, {

#========================================================
# constants
#========================================================

.EXCEL <- 1;
.RELOAD <- 1022;
.CLOSECONSOLE <- 1023;

.CALLBACK <- "BERT_Callback";

#========================================================
# functions - for general use
#========================================================

#--------------------------------------------------------
# close the console
#--------------------------------------------------------
CloseConsole <- function(){ invisible(.Call(.CALLBACK, .CLOSECONSOLE, 0, PACKAGE=.MODULE )); };

#--------------------------------------------------------
# reload the startup file
#--------------------------------------------------------
ReloadStartup <- function(){ .Call(.CALLBACK, .RELOAD, 0, PACKAGE=.MODULE ); };

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
# class type representing an Excel cell reference.
#========================================================

setClass( "xlReference", 
	representation( R1 = "integer", C1 = "integer", R2 = "integer", C2 = "integer", SheetID = "integer" ),
	prototype( R1 = 0L, C1 = 0L, R2 = 0L, C2 = 0L, SheetID = c(0L,0L))
	);

suppressMessages(setMethod( "nrow", "xlReference", function(x){ 
	if( x@R2 >= x@R1 ){ return( x@R2-x@R1+1 ); }
	else{ return(1); }
}, where = BERT));

suppressMessages(setMethod( "ncol", "xlReference", function(x){ 
	if( x@C2 >= x@C1 ){ return( x@C2-x@C1+1 ); }
	else{ return(1); }
}, where = BERT));

# length isn't really appropriate for this object
#setMethod( "length", "xlReference", function(x){ return(nrow(x) * ncol(x)); });

setMethod( "show", "xlReference", function(object){
	cat( "Excel Reference ", "R", object@R1, "C", object@C1, sep="" );
	if( object@R2 >= object@R1 && object@C2 >= object@C1 
		&& ( object@R1 != object@R2 || object@C1 != object@C2 ))
	{
		cat( ":", "R", object@R2, "C", object@C2, sep="" );
	}
	if( object@SheetID[1] != 0 || object@SheetID[2] != 0 )
	{
		cat( " SheetID ", object@SheetID[1], ".", object@SheetID[2], sep="");
	}
	cat( "\n" );
});

}); # end with(BERT)

#========================================================
# overload quit method or it will stop the excel process
#========================================================

quit <- function(){ BERT$CloseConsole() }
q <- quit;



