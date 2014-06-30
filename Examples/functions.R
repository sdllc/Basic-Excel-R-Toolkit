
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
# arbitrary complex function.  see
# http://stackoverflow.com/questions/6807068/why-is-my-recursive-function-so-slow-in-r
#
fibonacci <- local({
    memo <- c(1, 1, rep(NA, 100))
    f <- function(x) {
        if(x == 0) return(0)
        if(x < 0) return(NA)
        if(x > length(memo))
        stop("’x’ too big for implementation")
        if(!is.na(memo[x])) return(memo[x])
        ans <- f(x-2) + f(x-1)
        memo[x] <<- ans
        ans
    }
})

# source( file.path( BERT$HOME, "ExcelFunctions.R" ));


