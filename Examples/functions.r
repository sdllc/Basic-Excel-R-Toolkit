
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
