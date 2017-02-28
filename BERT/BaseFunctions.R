
with( BERT, {

#========================================================
# constants
#========================================================

.EXCEL <- 1;

.ADD_USER_BUTTON <- 100;
.CLEAR_USER_BUTTONS <- 101;

.HISTORY <- 200;
.REMAP_FUNCTIONS <- 300;

.WATCHFILES <- 1020;
.CLEAR <- 1021;
.RELOAD <- 1022;
.CLOSECONSOLE <- 1023;

.PROGRESSBAR <- 2000;
.DOWNLOAD <- 2001;

.CALLBACK <- "BERT_Callback";
# .COM_CALLBACK <- "BERT_COM_Callback";

#========================================================
# functions - for general use
#========================================================

#--------------------------------------------------------
# clear buffer.  if you want, you can map this to a 
# 'clear()' function.
#--------------------------------------------------------
ClearBuffer <- function(){ invisible(.Call(.CALLBACK, .CLEAR, 0, PACKAGE=.MODULE )); };

#--------------------------------------------------------
# close the console
#--------------------------------------------------------
CloseConsole <- function(){ invisible(.Call(.CALLBACK, .CLOSECONSOLE, 0, PACKAGE=.MODULE )); };

#--------------------------------------------------------
# reload startup files
#--------------------------------------------------------
ReloadStartup <- function(){ .Call(.CALLBACK, .RELOAD, 0, PACKAGE=.MODULE ); };

#--------------------------------------------------------
# reload functions into excel.  this assumes that the 
# functions have already been sourced() into R.
#--------------------------------------------------------
RemapFunctions <- function(){ .Call( .CALLBACK, .REMAP_FUNCTIONS, 0, PACKAGE=.MODULE ); };

#========================================================
# explicit function registration
#========================================================

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
		if( is.function( get( name, envir=env ))){
			fname <- paste0( prefix, name );
			assign( fname, list( name=fname, expr=name, envir=env, category=category ), envir=.function.map );
			count <<- count + 1;
		}
	});
	if( count > 0 ){ .Call( .CALLBACK, .REMAP_FUNCTIONS, 0, PACKAGE=.MODULE ); }
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
	.Call( .CALLBACK, .REMAP_FUNCTIONS, 0, PACKAGE=.MODULE );
}

#========================================================
# API for user buttons
#========================================================

.UserButtonCallbacks <- list();
.UserButtonCallback <- function(index){
	return(.UserButtonCallbacks[[index+1]]$FUN());
}

#--------------------------------------------------------
# add a user button.  these are added to the ribbon,
# there's a max of 6.  callback is an R function.
#--------------------------------------------------------
AddUserButton <- function( label, FUN, imageMso = NULL ){

	ubc <- structure( list(), class = "UserButtonCallback" );
	ubc$label <- label;
	ubc$FUN <- FUN;
	ubc$imageMso <- imageMso;

	.Call(.CALLBACK, .ADD_USER_BUTTON, c( label, "BERT$.UserButtonCallback", imageMso ), 0, PACKAGE=.MODULE);

	len <- length( .UserButtonCallbacks );
	.UserButtonCallbacks[[len+1]] <<- ubc;
}

#--------------------------------------------------------
# list user buttons
#--------------------------------------------------------
ListUserButtons <- function(){
	print( .UserButtonCallbacks );
}

#--------------------------------------------------------
# remove user buttons
#--------------------------------------------------------
ClearUserButtons <- function(){
	.UserButtonCallbacks <<- list();
	.Call(.CALLBACK, .CLEAR_USER_BUTTONS, 0, 0, PACKAGE=.MODULE);
}

#========================================================
# API for the file watch utility
#========================================================

.WatchedFiles <- new.env();

.RestartWatch <- function(){

	# we watch the functions dir by default.  you can overload behavior by
	# watching the folder with a specific function.

	path = gsub( "\\\\+$", "", gsub( "/", "\\\\", tolower(normalizePath(BERT$STARTUP_FOLDER))));
	if( !exists( path, envir=.WatchedFiles )) .WatchedFiles[[path]] = NULL;

	rslt <- .Call( BERT$.CALLBACK, BERT$.WATCHFILES, ls(.WatchedFiles), 0, PACKAGE=BERT$.MODULE );
	if( !rslt ){
		cat( "File watch failed.  Make sure the files you are watching exist and are readable.\n");
	}
}
.RestartWatch();

.ExecWatchCallback <- function( path ){

	path = gsub( "\\\\+$", "", gsub( "/", "\\\\", tolower(normalizePath(path)) ))

	# it's possible that both the directory and the specific file are 
	# being watched.  we want to support that pattern.  check the directory
	# first and execute (call with the path to the changed file)

	dir = gsub( "\\\\+$", "", gsub( "/", "\\\\", dirname(path)))
	if( exists( dir, envir=.WatchedFiles )){
		FUN = .WatchedFiles[[dir]];
		if( is.null(FUN)){
			FUN = function(a){
				if( grepl( "\\.(?:rscript|r|rsrc)$", a, ignore.case=T )){
					source(a, chdir=T);
					BERT$RemapFunctions();
				}
				else {
					cat("skipping file (invalid extension)\n");
				}
			}
		}
		cat(paste("Executing code on file change:", path, "\n" ));
		do.call(FUN, list(path));
	}

	# now look for the specific file and execute that function.
	# for backwards-compatibility purposes there are no arguments.

	if( exists( path, envir=.WatchedFiles )){
		FUN = .WatchedFiles[[path]];
		if( is.null(FUN)){
			FUN = function(a=path){
				if( grepl( "\\.(?:rscript|r|rsrc)$", a, ignore.case=T )){
					source(a, chdir=T);
					BERT$RemapFunctions();
				}
				else {
					cat("skipping file (invalid extension)\n");
				}
			}
		}
		cat(paste("Executing code on file change:", path, "\n" ));
		do.call(FUN, list());
	}

}

#--------------------------------------------------------
# watch file, execute code on change
#--------------------------------------------------------
WatchFile <- function( path, FUN=NULL, apply.now=F ){
	path = gsub( "\\\\+$", "", gsub( "/", "\\\\", tolower(normalizePath(path)) ));
	.WatchedFiles[[path]] = FUN;
	.RestartWatch();
} 

#--------------------------------------------------------
# stop watching file (by path)
#--------------------------------------------------------
UnwatchFile <- function( path ){
	path = gsub( "\\\\+$", "", gsub( "/", "\\\\", tolower(normalizePath(path)) ))
	rm( list=path, envir=.WatchedFiles );
	.RestartWatch();
}

#--------------------------------------------------------
# remove all watches (except default)
#--------------------------------------------------------
ClearWatches <- function(){
	rm( list=ls(.WatchedFiles), envir=.WatchedFiles );
	.RestartWatch();
}

#--------------------------------------------------------
# list watches - useful if something unexpected is happening
#--------------------------------------------------------
ListWatches <- function(){
	ls(.WatchedFiles);
}

#--------------------------------------------------------
# Excel callback function. Be careful with this unless 
# you know what you are doing.
#--------------------------------------------------------
.Excel<- function( command, arguments = list() ){ .Call(.CALLBACK, .EXCEL, command, arguments, PACKAGE=.MODULE ); };

}); # end with(BERT)

#========================================================
# utility methods and types
#========================================================

BERT$Util <- new.env();
with( BERT$Util, {


}); # end with(BERT$Util)

suppressMessages(attach( BERT$Util ));



