
#====================================================================
#
# R functions exposed in Excel.  These are just some simple 
# examples.  See the page
#
# http://bert-toolkit.org/example-functions
#
# for more examples.  You can add functions directly to this
# file, but it's probably better to put them in a separate file
# and then source() that file.  
#
#====================================================================

#
# performance testing
#
NOOP <- function( a, b ){ 0 }

#
# using arrays - output is a range of d x d
#
Identity <- function( d ){ diag(d); }

#
# matrix types (input is a range; should be REAL)
#
EigenValues <- function(x) 
{
	E <- eigen(x) 
	E$values
}

#
# matrix types (input is a range; should be REAL)
#
EigenVectors <- function(x) 
{
	E <- eigen(x) 
	E$vectors
}

#
# variable arguments (...)
#
SUM <- function( ... )
{
	sum(...);
}

#
# data frames - output is a range
#
cars <- function(){mtcars}

#
# utility function
#
matrix.to.frame <- function( mat )
{
	r = nrow(mat);
	c = ncol(mat);
	data <- as.data.frame( mat[2:r,2:c] );
	colnames(data) <- mat[1,2:c];
	rownames(data) <- mat[2:r,1];
	data;
}

#
# include Excel functions for the console
#
source( "ExcelFunctions.R" );

# add custom documentation to the Excel "insert function" dialog for 
# exported R functions using attributes.  NOTE: this is experimental
# and the interface may change in the future.

documented.function <- function( A, B ){
	A + B;
}

attributes( documented.function ) <- list( description=list( 
	"Example of documenting a function.  It will add arguments together.", 
	A="A number", 
	B="Another number"
));

# this is the new file watcher utility.  uncomment the line below and
# this file will be reloaded any time it's saved.  by default, it will run
# BERT$ReloadStartup, but you can pass a function to run any arbitrary
# R code when the file changes.

# BERT$WatchFile( file.path( BERT$HOME, "functions.R" ))



