
#====================================================================
#
# R functions exposed in Excel. These are just some basic
# examples. See the page
#
# https://bert-toolkit.com/example-functions
#
# for more.
#
# You can add your functions to this file, or add new 
# files to the startup folder. The startup folder is 
# `Documents\BERT\functions`. Any time you add or change
# a file in that folder, BERT will reload it automatically.
#
#====================================================================

#--------------------------------------------------------------------
#
# Functions in this file will be added to Excel. BERT will 
# add "R." to the name, so the first function will be called 
# "R.EigenValues" in Excel.
#
#--------------------------------------------------------------------

#
# first example: takes a matrix as input. we can't return a list,
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
# You can add documentation to your functions, which will show up 
# in the Excel insert function dialog (this is completely optional).  
# The attribute "description" is a list. The first (unnamed) entry 
# will be the main documentation. Named list entries match named 
# arguments.
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




