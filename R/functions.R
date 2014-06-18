
#
# performance testing
#
NOOP <- function( a, b ){ 0 }

#
# using arrays
#
SVD <- function( mat ){ svd(mat); }

#
# see
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

