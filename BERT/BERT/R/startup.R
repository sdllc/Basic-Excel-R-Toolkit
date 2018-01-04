
Cholesky <- function(mat){ chol(mat) }
attr( Cholesky, "category" ) <- "Linear Algebra";
attr( Cholesky, "description" ) <- list( "Cholesky Decomposition", mat="Input matrix (must be positive-definite)" );
fx2 <- function(...){ sum(202,...) }
zeb <- function(a, b=2, c=3){ a+b+c }

BERT <- new.env();
with( BERT, {

    list.functions <- function(envir=.GlobalEnv){
        funcs <- ls(envir=envir, all.names=F);
        funcs <- funcs[sapply(funcs, function(a){ mode(get(a, envir=envir))=="function"; })];
        z <- lapply(funcs, function(a){
            func <- get(a, envir=envir);
            f <- formals(func);
            attrib <- attributes(func)[names(attributes(func)) != "srcref" ];
            list(name=a, arguments=lapply(names(f), function(b){ list(name=b, default=f[[b]])}), attributes=attrib);
        });
        z;
    }

    install.com.pointer <- function(key, descriptor){
        env <- BERTModule:::.WrapDispatch2(key, descriptor$interface );
        lapply( descriptor$functions, function(method){
            name <- method$name;
            if( method$type == "get" ){ name <- paste0( "get_", name ); }
            if( method$type == "put" ){ name <- paste0( "put_", name ); }
            BERTModule:::.DefineCOMFunc( name, method$name, func.type=method$type, func.args=unlist(method$arguments), target.env=env );
        });
        return(env);
    }

    install.application.pointer <- function(key, descriptor){

        excel.env <- new.env();

        #env <- BERTModule:::.WrapDispatch2(key, descriptor$interface );

        #lapply( descriptor$functions, function(method){
        #    name <- method$name;
        #    if( method$type == "get" ){ name <- paste0( "get_", name ); }
        #    if( method$type == "put" ){ name <- paste0( "put_", name ); }
        #    BERTModule:::.DefineCOMFunc( name, method$name, func.type=method$type, func.args=unlist(method$arguments), target.env=env );
        #});

        assign( "Application", install.com.pointer(key, descriptor), envir=excel.env);

        lapply( descriptor$enums, function(enum){
            enum.env <- new.env();
            lapply( names(enum$values), function( name ){ assign( name, enum$values[name], envir=enum.env ); });
            assign( enum$name, enum.env, envir=excel.env );
        })

        assign( "EXCEL", excel.env, envir=.GlobalEnv);
    }

});
#BERT$
#list.functions();

pb.test <- function(){
    p <- txtProgressBar(min=0, max=100, style=3);
    for( i in 1:100){
        setTxtProgressBar(p, i);
        Sys.sleep(.1);
    }
    close(p);
}

print.mat <- function(mat){
	print(mat);
	print("\n");
	T;
} 

check.t <- function(mat){
	print(t(mat));
	print("\n");
	T;
}

test.beep <- function(){

    BERTModule:::.Callback("excel", list(0x8000));

}

SB.Test <- function(){
    BERTModule:::.Callback("status", "fish");
}

How.Big <- function(){

  # use the Excel API function to get a reference to the
  # caller.  for a range, that will include the top-left
  # and bottom-right cells

#  ref <- BERT$.Excel(89); # xlfCaller
   ref <- BERTModule:::.Callback("excel", list(89));

  # use our functions to get the size of the range, and then
  # paste together a string

  n.rows <- nrow(ref);
  n.cols <- ncol(ref);

  return(paste(n.rows, "x", n.cols));

}
