
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
Excel<- function( command, arguments = list() ){ .Call(.CALLBACK, .EXCEL, command, arguments, PACKAGE=.MODULE ); };

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
	representation( r1 = "integer", c1 = "integer", r2 = "integer", c2 = "integer", sheetID = "integer" ),
	prototype( r1 = 0L, c1 = 0L, r2 = 0L, c2 = 0L, sheetID = c(0L,0L))
	);

setMethod( "show", "xlReference", function(object){
	cat( "Excel Reference ", "R", object@r1, "C", object@c1, sep="" );
	if( object@r2 >= object@r1 && object@c2 >= object@c1 
		&& ( object@r1 != object@r2 || object@c1 != object@c2 ))
	{
		cat( ":", "R", object@r2, "C", object@c2, sep="" );
	}
	if( object@sheetID[1] != 0 || object@sheetID[2] != 0 )
	{
		cat( " SheetID ", object@sheetID[1], ".", object@sheetID[2], sep="");
	}
	cat( "\n" );
});

}); # end with(BERT)

#========================================================
# overload quit method or it will stop the excel process
#========================================================

quit <- function(){ BERT$CloseConsole() }
q <- quit;



