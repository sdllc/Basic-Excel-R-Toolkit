#
# Copyright (c) 2017-2018 Structured Data, LLC
# 
# This file is part of BERT.
#
# BERT is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# BERT is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with BERT.  If not, see <http://www.gnu.org/licenses/>.
#

#===============================================================================
#
# load module. moved from code.
#
#===============================================================================

library(BERTModule, lib.loc=paste0(Sys.getenv("BERT_HOME"), "module"));

#===============================================================================
#
# first we create the BERT environment with utility functions
#
#===============================================================================

(function(){

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
    # rebuild the functions map
    #
    remap.functions <- function(){
      .Call("BERT.Callback", "remap-functions", list(), PACKAGE="(embedding)");
    }

    #===========================================================================
    #
    # user buttons 
    #
    #===========================================================================

    .user.button.env <- new.env();

    AddUserButton <- function(label, FUN, image.mso = "R", tip=""){
      id <- .Call("BERT.Callback", "add-user-button", list(label, image.mso, tip), PACKAGE="(embedding)");
      if(!is.numeric(id) || id == 0){
        stop("add button failed");
      }
      .user.button.env[[toString(id)]] = list(label=label, FUN=FUN, image.mso=image.mso, id=id, tip=tip);
      return(id);
    }

    ClearUserButtons <- function(){
      .Call("BERT.Callback", "clear-user-buttons", list(), PACKAGE="(embedding)");
      rm(list=ls(.user.button.env), envir=.user.button.env);
    }

    RemoveUserButton <- function(id){
      .Call("BERT.Callback", "remove-user-button", list(id), PACKAGE="(embedding)");
      rm(toString(id), envir=.user.button.env);
    }

    ExecUserButton <- function(id){
      button <- .user.button.env[[toString(id)]];
      button$FUN();
    }

    #===========================================================================
    #
    # object cache
    #
    #===========================================================================

    .object.cache.env <- new.env();
    .cache.token <- 1000;

    setClass( "BERTCacheReference", 
      slots = c(reference = "numeric"),
      prototype = list( reference = 0 ));

    return.cache.reference <- function(obj){
      token <- BERT$.cache.token;
      assign(".cache.token", envir=BERT, token+1);
      .object.cache.env[[toString(token)]] = obj;
      new("BERTCacheReference", reference=token)
    }

    .get.cached.object <- function(ref){
      .object.cache.env[[toString(ref)]];
    }

    #===========================================================================

    .function.map <- new.env();

    #--------------------------------------------------------
    # map all functions in an environment.  the ... arguments
    # are passed to ls(), so use pattern='X' to subset 
    # functions in the environment. 
    #--------------------------------------------------------
    UseEnvironment <- function(env, prefix, category, ...){
      count <- 0;
      if( missing( prefix )){ prefix = ""; }
      else { prefix = paste0( prefix, "." ); }
      if( missing( category )){ category = ""; }
      if(is.character(env)){ env = as.environment(env); }
      lapply( ls( env, ... ), function( name ){
        func <- get( name, envir=env );
        if( is.function(func)){
          func.formals <- formals(func);
          arguments=lapply(names(func.formals), function(b){ list(name=b, default=func.formals[[b]])});
          fname <- paste0( prefix, name );
          assign( fname, list( name=fname, expr=name, envir=env, category=category, arguments=arguments ), envir=.function.map );
          count <<- count + 1;
        }
      });
      if( count > 0 ){ 
        remap.functions();
      }
      return( count > 0 );
    }

    #--------------------------------------------------------
    # this is an alias for UseEnvironment that prepends the
    # package: for convenience.
    #--------------------------------------------------------
    UsePackage <- function( pkg, prefix, category, ... ){
      require( pkg, character.only=T );
      UseEnvironment( paste0( "package:", pkg ), prefix, category, ... );
    }

    #--------------------------------------------------------
    # remove mapped environment/package functions
    #--------------------------------------------------------
    ClearMappedFunctions <- function(){
      rm( list=ls(.function.map), envir=.function.map );
      remap.functions();
    }

    #--------------------------------------------------------
    # pass through
    #--------------------------------------------------------
    .call.mapped.function <- function(name, ...){
      ref <- BERT$.function.map[[name]];
      do.call(ref$expr, list(...), envir=ref$envir);
    }

    #===========================================================================

    #
    # autocomplete for the console/shell. we add a custom completer later.
    # 
    # FIXME: are we using this one or the one in the module? and why are there
    # two?
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
    # lists functions in the global environment (or specified environment) 
    # for loading into Excel. using custom environments is not enabled in 
    # BERT2, but we're not deprecating it yet -- it might come back.
    #
    list.functions <- function(envir=.GlobalEnv){
      funcs <- ls(envir=envir, all.names=F);
      if(length(funcs) == 0){ return(NULL); }
      funcs <- funcs[sapply(funcs, function(a){ mode(get(a, envir=envir))=="function"; })];

      function.list <- lapply(funcs, function(a){
        func <- get(a, envir=envir);
        f <- formals(func);
        attrib <- attributes(func)[names(attributes(func)) != "srcref" ];
        list(name=a, flags=0, arguments=lapply(names(f), function(b){ 
          dflt <- "";
          dflt.type <- typeof(f[[b]]);
          if(dflt.type == "language"){ dflt <- capture.output(f[[b]]); }
          else if(dflt.type == "character"){ dflt <- paste0('"', f[[b]], '"'); }
          else if(dflt.type != "symbol"){ dflt <- f[[b]]; }
          list(name=b, default=dflt);
        }), attributes=attrib);
      });

      # mapped functions 
      mapped.list <- lapply(BERT$.function.map, function(a){
        list(name=a$name, flags=1, arguments=a$arguments, attributes=list(category=a$category));
      });
      function.list <- c(function.list, mapped.list);

      class(function.list) <- "exported.function.list"; # see below
      function.list;
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
        tmp <- new.env();
        src <- descriptor$enums[[name]];
        sapply(names(src), function(x){ assign(x, src[[x]], envir=tmp) });
        assign( name, tmp, envir=env)
      });
      attach(list(EXCEL=env));
    }

  });

  #===========================================================================

  BERT.version <- (function(){
    version.string <- Sys.getenv('BERT_VERSION');
    version <- list();
    version['build.date'] <- Sys.getenv('BERT_BUILD_DATE');
    parts <- as.numeric(unlist(strsplit(version.string, '\\.')));
    version['major'] <- parts[1];
    version['minor'] <- parts[2];
    version['patch'] <- parts[3];
    version['version.string'] <- version.string;
    return(version);
  })();


  #
  # helpful for dev, can go
  #
  print.exported.function.list <- function(a){
    cat("\nExported functions:\n\n");
    invisible(sapply(a, function(func){
      arg.list = sapply(func$arguments, function(arg){
        if(is.null(arg$default) || arg$default == ""){
          return(arg$name);
        }
        paste0(arg$name, "=", arg$default);
      });
      cat(paste0(" ", "(", func$flags, ") ", func$name, "(", paste(arg.list, collapse=", "), ")\n"));
    }));
    cat("\n");
  }

  attach(environment());

  #-----------------------------------------------------------------------------
  # autocomplete
  #-----------------------------------------------------------------------------

  #
  # this is a monkeypatch for the existing R autocomplete # functionality. we are making two 
  # changes: (1) for functions, store the signagure for use as a call tip. (2) for functions 
  # within environments, resolve and get parameters.
  #
  # update: now delegating file completion to C (probably more to come).
  #
  .CustomCompleter <- function(.CompletionEnv){

    .fqFunc <- function (line, cursor=-1) 
    {
      localBreakRE <- "[^\\.\\w\\$\\@\\:]";

      if( cursor == -1 ){ cursor = nchar(line); }

        parens <- sapply(c("(", ")"), function(s) gregexpr(s, substr(line, 
      1L, cursor), fixed = TRUE)[[1L]], simplify = FALSE)
        parens <- lapply(parens, function(x) x[x > 0])
          
        
        temp <- data.frame(i = c(parens[["("]], parens[[")"]]), c = rep(c(1, 
      -1), lengths(parens)))
        if (nrow(temp) == 0) 
      return(character())
      
        temp <- temp[order(-temp$i), , drop = FALSE]
        wp <- which(cumsum(temp$c) > 0)

        if (length(wp)) {
      index <- temp$i[wp[1L]]
      prefix <- substr(line, 1L, index - 1L)
      suffix <- substr(line, index + 1L, cursor + 1L)
      
      if ((length(grep("=", suffix, fixed = TRUE)) == 0L) && 
          (length(grep(",", suffix, fixed = TRUE)) == 0L)) 
          utils:::setIsFirstArg(TRUE)
      if ((length(grep("=", suffix, fixed = TRUE))) && (length(grep(",", 
          substr(suffix, utils:::tail.default(gregexpr("=", suffix, 
        fixed = TRUE)[[1L]], 1L), 1000000L), fixed = TRUE)) == 
          0L)) {
          return(character())
      }
      else {
          possible <- suppressWarnings(strsplit(prefix, localBreakRE, 
        perl = TRUE))[[1L]]
          possible <- possible[nzchar(possible)]
          if (length(possible)) 
        return(utils:::tail.default(possible, 1))
          else return(character())
      }
        }
        else {
      return(character())
        }
    }

    .fqFunctionArgs <- function (fun, text, S3methods = utils:::.CompletionEnv$settings[["S3"]], 
        S4methods = FALSE, add.args = rc.getOption("funarg.suffix")) 
    {
    
      .resolveObject <- function( name ){

        p <- environment();
        n <- unlist( strsplit( name, "[^\\w\\.,]", F, T ));
        while( length( n ) > 1 ){
          if( n == "" || !exists( n[1], where=p )) return( NULL );
          p <- get( n[1], envir=p );
          n <- n[-1];
        }
        if( n == "" || !exists( n[1], where=p )) return( NULL );
        list( name=n[1], fun=get( n[1], envir=p ));
      }
    
      .function.signature <- function(fun){
        x <- capture.output( args(fun));
        paste(trimws(x[-length(x)]), collapse=" ");
      }
    
      .fqArgNames <- function (fname, use.arg.db = utils:::.CompletionEnv$settings[["argdb"]]) 
      {
        funlist <- .resolveObject( fname );
        fun <- funlist$fun;
        if( !is.null(fun) && is.function(fun )) { 
          env <- utils:::.CompletionEnv;
          env$function.signature <- sub( '^function ', paste0( funlist$name, ' ' ), .function.signature(fun));
          return(names( formals( fun ))); 
        }
        return( character());
      };

      if (length(fun) < 1L || any(fun == "")) 
        return(character())
          specialFunArgs <- utils:::specialFunctionArgs(fun, text)
      if (S3methods && exists(fun, mode = "function")) 
        fun <- c(fun, tryCatch(methods(fun), warning = function(w) {
        }, error = function(e) {
        }))
      if (S4methods) 
        warning("cannot handle S4 methods yet")
      allArgs <- unique(unlist(lapply(fun, .fqArgNames)))
      ans <- utils:::findMatches(sprintf("^%s", utils:::makeRegexpSafe(text)), 
        allArgs)
      if (length(ans) && !is.null(add.args)) 
        ans <- sprintf("%s%s", ans, add.args)
      c(specialFunArgs, ans)
    }

    .CompletionEnv[["function.signature"]] <- "";
    .CompletionEnv[["in.quotes"]] <- F;

        text <- .CompletionEnv[["token"]]
        if (utils:::isInsideQuotes()) {
      {
          .CompletionEnv[["comps"]] <- character()
        .CompletionEnv[["in.quotes"]] <- T;
          utils:::.setFileComp(TRUE)
      }
        }
        else {
      utils:::.setFileComp(FALSE)
      utils:::setIsFirstArg(FALSE)
      guessedFunction <- if (.CompletionEnv$settings[["args"]]) 
          .fqFunc(.CompletionEnv[["linebuffer"]], .CompletionEnv[["start"]])
      else ""
      
      .CompletionEnv[["fguess"]] <- guessedFunction
      fargComps <- .fqFunctionArgs(guessedFunction, text)
      
      if (utils:::getIsFirstArg() && length(guessedFunction) && guessedFunction %in% 
          c("library", "require", "data")) {
          .CompletionEnv[["comps"]] <- fargComps
          return()
      }
      lastArithOp <- utils:::tail.default(gregexpr("[\"'^/*+-]", text)[[1L]], 
          1)
      if (haveArithOp <- (lastArithOp > 0)) {
          prefix <- substr(text, 1L, lastArithOp)
          text <- substr(text, lastArithOp + 1L, 1000000L)
      }
      spl <- utils:::specialOpLocs(text)
      comps <- if (length(spl)) 
          utils:::specialCompletions(text, spl)
      else {
          appendFunctionSuffix <- !any(guessedFunction %in% 
        c("help", "args", "formals", "example", "do.call", 
          "environment", "page", "apply", "sapply", "lapply", 
          "tapply", "mapply", "methods", "fix", "edit"))
          utils:::normalCompletions(text, check.mode = appendFunctionSuffix)
      }
      if (haveArithOp && length(comps)) {
          comps <- paste0(prefix, comps)
      }
      comps <- c(fargComps, comps)
      .CompletionEnv[["comps"]] <- comps
        }
  };

  rc.options( custom.completer=.CustomCompleter );

  #
  # banner
  #

  cat(paste0("---\n",
      "BERT Version ", Sys.getenv("BERT_VERSION"), " (http://bert-toolkit.com).\n\n"));

})();

