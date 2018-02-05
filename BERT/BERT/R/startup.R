
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

    install.com.pointer <- function(descriptor){

        # this way of doing it uses a closure for each method. that's probably fine, but it 
        # seems awkward -- especially for the external pointer. it's also unecessary, since 
        # we have the env, we can stick the pointer in there and refer to it. 

        # these are also hard to read, since there's no identifying information in the 
        # function body [What other information is there? the name is the same]

        env <- new.env();
        lapply( sort(names(descriptor$functions)), function(name){
          ref <- descriptor$functions[name][[1]]
          if(length(ref$arguments) == 0){
            func <- eval(bquote(function(...){
              .Call("COMCallback", .(ref$name), .(ref$call.type), 
                .(ref$index), .(descriptor$pointer), list(...), PACKAGE="BERTModule" );
            }));
          }
          else {
            func <- eval(bquote(function(){
			        .Call("COMCallback", .(ref$name), .(ref$call.type), 
                .(ref$index), .(descriptor$pointer), c(as.list(environment())), 
                PACKAGE="BERTModule" );
            }));
            arguments.expr <- paste( "alist(", paste( sapply( ref$arguments, function(x){ paste( x, "=", sep="" )}), collapse=", " ), ")" );
            formals(func) <- eval(parse(text=arguments.expr));
          }
          assign(name, func, env=env);
        });

        class(env) <- c("IDispatch", descriptor$interface);
        env;

    }

    #
    # when installing the base pointer, we include enums (and there are a lot of them)
    #
    install.application.pointer <- function(descriptor){
      assign( "descriptor", descriptor, env=.GlobalEnv ); # dev
      env <- new.env();
      assign( "Application", install.com.pointer(descriptor), envir=env);
      lapply(names(descriptor$enums), function(name){
        assign( name, descriptor$enums[name], envir=env)
      });
      assign( "EXCEL", env, envir=.GlobalEnv);
    }

});
