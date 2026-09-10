
#
# add all arguments
#
TestAdd <- function(...){
  sum(...)
}

#
# eigenvalues for matrix (returns a vector)
#
EigenValues <- function(mat){
  eigen(mat)$values
}

#
# you can describe your functions to Excel. the description attribute is a
# character vector: the function first, then its arguments in order. the
# description appears in the Insert Function dialog (the fx button), and
# each argument's description in the Function Arguments dialog, which opens
# if you type =R.EigenValues( in a cell and press Ctrl+A.
#
# see docs/FUNCTION-HELP.md for the rest, including how to get the tooltip
# that appears as you type.
#

attr(TestAdd, "description") <- "Adds all of its arguments"

attr(EigenValues, "description") <- c(
  "Eigenvalues of a matrix, as a vector",
  "a square matrix")

attr(EigenValues, "category") <- "Example R Functions"
