
#====================================================================
#
# R functions exposed in Excel.  These are just some simple 
# examples.  See the page
#
# https://bert-toolkit.com/example-functions
#
# for more examples.  You can add functions directly to this
# file, but it's probably better to put them in a separate file
# and then source() that file.  
#
#====================================================================

#--------------------------------------------------------------------
#
# 1. The basics: adding functions to Excel.  Any function in this 
#    file (or in a file you include using source()) will be added to 
#    Excel.  BERT will add "R." to the name, so the first function 
#    will be called "R.EigenValues" in Excel.
#
#--------------------------------------------------------------------

#
# first example: takes a matrix as input.  we can't return a list,
# so here we just return the eigenvalues from the result.
#
EigenValues <- function(mat) {
  E <- eigen(mat) 
  E$values
}

#
# the same thing, but for the vectors.
#
EigenVectors <- function(mat) {
  E <- eigen(mat) 
  E$vectors
}

#
# you can use ... (three-dots) arguments to pass a variable 
# number of parameters to the function.
#
Add <- function( ... ) {
  sum(...);
}

#--------------------------------------------------------------------
#
# 2. Documentation: you can add documentation to your functions, 
#    which will show up in the Excel insert function dialog.  The
#    attribute "description" is a list.  The first (unnamed) entry
#    will be the main documentation.  Named list entries match 
#    named arguments.
#
#--------------------------------------------------------------------

Example.Function <- function(A, B) {
  A + B;
}

attr( Example.Function, "description" ) <- list( 
  "This function adds two arguments together", 
  A="A number", 
  B="Another number"
);

#--------------------------------------------------------------------
#
# 3. Including files: use source() to include other code files.
#    The "ExcelFunctions.R" file includes a number of useful 
#    functions for calling the Excel API.  See the website for 
#    documentation and examples.
#
#--------------------------------------------------------------------

source( "ExcelFunctions.R" );

#--------------------------------------------------------------------
#
# 4. Watching files: with the code below, BERT will reload this 
#    file whenever you save changes.  To turn that off, remove 
#    the code.  You can watch other files too, and you can execute
#    any function when the file changes.
#
#--------------------------------------------------------------------

BERT$WatchFile( file.path( BERT$HOME, "functions.R" ))





