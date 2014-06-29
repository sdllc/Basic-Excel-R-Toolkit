
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

Namespace <- function(a, b){ return( paste( b, "$", a, sep="" )); }
WordList <- function(){ 
	wl <- vector();
	for( i in search()){ wl <- c( wl, ls(i, all.names=1)); }
	for( i in ls(.GlobalEnv))
	{
		if( is.environment(get(i, .GlobalEnv)))
		{
			wl <- c( wl, unlist( lapply( ls(get(i, .GlobalEnv), all.names=1), Namespace, i)));	
		}
	}
	wl;
}

#========================================================
# class type representing an Excel cell reference.
#========================================================

xlRefClass <- function(r1 = 0, c1 = 0, r2 = 0, c2 = 0, sheetID = vector( mode="integer", length=2)) {
    r = list(
        r1 = r1,
        r2 = r2,
        c1 = c1,
		c2 = c2,
		sheetID = sheetID
		);

    r <- list2env(r)
    class(r) <- "xlRefClass"
    return(r)
}

});

print.xlRefClass <- function(x){ 
	if( x$r2 <= x$r1 && x$c2 <= x$c1 )
	{
		s <- paste( "Excel Reference R", x$r1, "C", x$c1, sep="");
	}
	else { s <- paste( "Excel Reference R", x$r1, "C", x$c1, ":R", x$r2, "C", x$c2, sep=""); }
	cat( s, " ", x$sheetID, "\n", sep="");
}


#========================================================
# overload quit method or it will stop the excel process
#========================================================

quit <- function(){ BERT$CloseConsole() }
q <- quit;



