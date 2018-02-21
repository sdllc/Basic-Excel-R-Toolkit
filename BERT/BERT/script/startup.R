

#===============================================================================
#
# load module. moved from code.
#
#===============================================================================
library(BERTModule, lib.loc=paste0(Sys.getenv("BERT2_HOME_DIRECTORY"), "module"));

#===============================================================================
#
# first we create the BERT environment with utility functions
#
#===============================================================================

BERT <- new.env();
with( BERT, {

  #
  # calls Excel API. this is the programmatic (i.e. non-COM) API. command
  # is an integer, arguments are dependent on the command. see (e.g.) the 
  # HowBig function.
  #
  # this function should move to the module, but we're leaving it here 
  # (temporarily) for backwards compatibility. TODO: deprecate.
  #
  .Excel <- function(command, ...) {
    .Call("BERT.Callback", "excel", list(command, ...), PACKAGE="(embedding)");
  }


  #
  # autocomplete for the console/shell. we add a custom completer later.
  #
  .Autocomplete <- function(...){
    
    ac <- utils:::.win32consoleCompletion(...);
    if( length( utils:::.CompletionEnv$comps) > 0 ){
      ac$comps <- paste( utils:::.CompletionEnv$comps, collapse='\n' );
    }

    ac$function.signature <- ifelse( is.null( utils:::.CompletionEnv$function.signature ), "", utils:::.CompletionEnv$function.signature );
    ac$token <- ifelse( is.null( utils:::.CompletionEnv$token ), "", utils:::.CompletionEnv$token );
    ac$fguess <- ifelse( is.null( utils:::.CompletionEnv$fguess ), "", utils:::.CompletionEnv$fguess );
    ac$start <- utils:::.CompletionEnv$start;
    ac$end <- utils:::.CompletionEnv$end;
    # ac$file.name <- utils:::.CompletionEnv$fileName;
    ac$in.quotes <- utils:::.CompletionEnv$in.quotes;

    ac;
  }

  #
  # basic callback function. command is a string, by convention, and 
  # data is usually a list of arguments (but can be omitted). 
  # 
  # FIXME: move this into a closure to prevent external access (?)
  #
  #.Callback <- function( command, data=NULL ){
  #  .Call("BERT.Callback", command, data, PACKAGE="(embedding)");
  #}

  #
  # lists functions in the global environment (or specified environment) 
  # for loading into Excel. using custom environments is not enabled in 
  # BERT2, but we're not deprecating it yet -- it might come back.
  #
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

  #
  # set up COM pointers for Excel objects, with function calls
  #
  install.com.pointer <- function(descriptor){

    # this way of doing it uses a closure for each method. that's probably 
    # fine, but it seems awkward -- especially for the external pointer. it's 
    # also unecessary, since we have the env, we can stick the pointer in 
    # there and refer to it. 

    # these are also hard to read, since there's no identifying information 
    # in the function body [What other information is there? the name is the
    # same]

    # OK this way they're easier to read, at least

    env <- new.env();
    lapply( sort(names(descriptor$functions)), function(name){
      ref <- descriptor$functions[name][[1]]
      if(length(ref$arguments) == 0){
        func <- eval(bquote(function(...){
          .Call("BERT.COMCallback", .(ref$name), .(ref$call.type), 
            .(ref$index), .(descriptor$pointer), list(...), PACKAGE="(embedding)" );
        }));
      }
      else {
        func <- eval(bquote(function(){
          .Call("BERT.COMCallback", .(ref$name), .(ref$call.type), 
            .(ref$index), .(descriptor$pointer), c(as.list(environment())), 
            PACKAGE="(embedding)" );
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

#
# banner
#

cat(paste0("---\n
BERT Version ", Sys.getenv("BERT_VERSION"), " (http://bert-toolkit.com).\n
"));
