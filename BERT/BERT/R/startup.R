
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
        env <- BERTModule:::.WrapDispatch3(key, descriptor$interface );
        lapply( descriptor$functions, function(method){
            name <- method$name;
            if( method$type == "get" ){ name <- paste0( "get_", name ); }
            if( method$type == "put" ){ name <- paste0( "put_", name ); }
            BERTModule:::.DefineCOMFunc( name, method$name, func.type=method$type, func.index=method$index, func.args=unlist(method$arguments), target.env=env );
        });
        return(env);
    }

    install.application.pointer <- function(key, descriptor){

        excel.env <- new.env();

        assign( "Application", install.com.pointer(key, descriptor), envir=excel.env);

        lapply( descriptor$enums, function(enum){
            enum.env <- new.env();
            lapply( names(enum$values), function( name ){ assign( name, enum$values[name], envir=enum.env ); });
            assign( enum$name, enum.env, envir=excel.env );
        })

        assign( "EXCEL", excel.env, envir=.GlobalEnv);
    }

});
