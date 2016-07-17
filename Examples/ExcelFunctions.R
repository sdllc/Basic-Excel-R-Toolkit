
#====================================================================
#
# Functions for interacting with the spreadsheet.
#
#====================================================================

#
# we're going to stick these functions in a separate namespace,
# BERT.Excel, and then attach that namespace.  we do that so they're
# available in the console but don't get added as Excel functions.
#

if( any(search() == "BERT.Excel")){
	suppressMessages(detach( BERT.Excel ));
}

BERT.Excel <- new.env();
with( BERT.Excel, {

#--------------------------------------------------------------------
#
# get a sheet ID by name.  sheet IDs are internal to Excel.  it's
# probably worth noting that these are not persistent across Excel
# sessions (they may be pointers?) but they are persistent as long
# as the workbook is open.
#
#--------------------------------------------------------------------
.GetSheetID <- function( sheetName )
{
	ref <- BERT$.Excel( 0x4004, list( sheetName ));
	return( unlist( ref@SheetID ));
}

#--------------------------------------------------------------------
#
# create a range from a text string (e.g. "A1"), potentially
# including the sheet name ("Sheet1!A2").  Also can be a defined
# name - this adds another lookup, but it's useful to have it
# in one place (the same way Excel works).
#
#--------------------------------------------------------------------
Text.Ref <- function( reference.text ) {

	# try a defined name.  this will convert it to a reference.
	nr <- BERT$.Excel( 107, list( paste("!", reference.text, sep="" )));
	if( !is.null( nr ) && !is.na( nr )){
		reference.text <- substring( nr, 2 );
	}

	# assume A1 input, but check? names (above) will return R1C1.
	ref <- BERT$.Excel( 147, list( reference.text, TRUE ));
	if( !inherits( ref, "xlReference" )){
		# in R1C1 notation, try again
		ref <- BERT$.Excel( 147, list( reference.text, FALSE ));
	}

	# this function (xlfTextref) returns a non-usable sheet ID.
	# not sure what it actually refers to, but don't use it
	ref@SheetID <- c(0L,0L);

	mm <- regexpr( "(.*?)\\!", reference.text )
	if( mm >= 0 )
	{
		sname <- substr( reference.text, mm, attr( mm, "match.length") - mm );
		sname <- sub( "^'(.*)'", "\\1", sname );
		ref@SheetID <- .GetSheetID( sname );
	}

	return( ref );
}


#--------------------------------------------------------------------
#
# verifies a range, or creates a range from text
#
#--------------------------------------------------------------------
Ensure.Ref <- function( ref )
{
	# ref is a string, try to convert
	if( is.character( ref )) ref <- Text.Ref( ref );

	# check ref typeGe
	if( !inherits( ref, "xlReference" )) stop( "Bad ref type" );

	return( ref );
}

#--------------------------------------------------------------------
#
# the excel API wants R1C1 only, so we need to convert A1 references.
#
#--------------------------------------------------------------------
Normalize.Formula <- function( formula.or.value ){
	if( typeof( formula.or.value ) == "character" 
		&& !is.na(formula.or.value) 
		&& length( formula.or.value ) > 0 
		&& substring( formula.or.value, 1, 1 ) == "=" ){
		coffset <- new("xlReference", R1=1L, C1=1L);
		formula.or.value <- BERT$.Excel( 241, list( formula.or.value, TRUE, FALSE, 1, coffset));
	}
	return( formula.or.value );
}

#--------------------------------------------------------------------
# 
# set a range of formulae (or values).
#
# if the value is singular, it will fill all cells in
# the range with the value.  if the value has nrow and ncol,
# it will attempt to fill it in the same shape.  if the value
# has length but not dimensions, then it will fill the range
# sequentially.
#
# repeat.values will recycle the value (default True), but 
# only if the value is a list or vector.  this has no effect 
# for shaped ranges.  if repeat.values is False, extra cells
# will be filled with default.value.  if the value has a shape,
# and that shape is smaller than the target range, extra rows
# and columns will be filled with default.value.
#
#--------------------------------------------------------------------
Set.Range.1 <- function( ref, formula.or.value, repeat.values=T, default.value="", column.first=F ){
	
	len <- length( formula.or.value );

	# case 1: single value (or formula)
	if( len == 1 ){
		Set.Cell( ref, formula.or.value );
	}
	else {
		ref <- Ensure.Ref( ref );
		range.len <- nrow(ref) * ncol(ref);
		target <- new("xlReference", R1=1L, C1=1L);
		
		n.row = nrow( formula.or.value );
		
		# case 2: shaped
		if( !is.null( n.row )){
			while( nrow( formula.or.value ) < nrow( ref )){ formula.or.value <- rbind( formula.or.value, default.value, deparse.level=0 ); }
			while( ncol( formula.or.value ) < ncol( ref )){ formula.or.value <- cbind( formula.or.value, default.value, deparse.level=0 ); }
			for( rx in seq( 0, nrow(ref) - 1 )){
				target@R1 <- ref@R1 + rx;
				for( cx in seq( 0, ncol(ref) - 1 )){
					target@C1 <- ref@C1 + cx;
					BERT$.Excel( 0x8000 + 96, list( Normalize.Formula(formula.or.value[rx+1,cx+1]), target ));
				}
			}
		}
		
		# case 3: list/vector
		else {
			index <- 1;
			
			if( range.len > len ){
				if( repeat.values ){
					formula.or.value <- c( rep( formula.or.value, length.out=range.len ));
				}
				else {
					formula.or.value <- c( formula.or.value, rep( default.value, length.out=range.len ));
				}
			}

			if( column.first ){
				for( cx in seq( 0, ncol(ref) - 1 )){
					target@C1 <- ref@C1 + cx;
					for( rx in seq( 0, nrow(ref) - 1 )){
						target@R1 <- ref@R1 + rx;
						BERT$.Excel( 0x8000 + 96, list( Normalize.Formula(formula.or.value[index]), target ));
						index <- index + 1;
					}
				}
			}
			else {
				for( rx in seq( 0, nrow(ref) - 1 )){
					target@R1 <- ref@R1 + rx;
					for( cx in seq( 0, ncol(ref) - 1 )){
						target@C1 <- ref@C1 + cx;
						BERT$.Excel( 0x8000 + 96, list( Normalize.Formula(formula.or.value[index]), target ));
						index <- index + 1;
					}
				}
			}
		}
	}

}

#--------------------------------------------------------------------
#
# set a cell formula (or value).  
#
# this function uses xlcFormulaFill. if the reference is
# a range, then it will set all cells to the same formula 
# (or value).  it won't fill a range with values from a 
# vector.
#
#--------------------------------------------------------------------
Set.Cell <- function( ref, formula.or.value ) {
	BERT$.Excel( 0x8000 + 97, list( Normalize.Formula( formula.or.value ), Ensure.Ref( ref ) ));
}

#--------------------------------------------------------------------
#
# like Set.Cell, but this version uses xlcFormulaArray.
# it's the same thing, but like pressing Ctrl+Alt+Enter.
# use if you are setting an array formula.
#
#--------------------------------------------------------------------
Set.Array <- function( ref, formula.or.value ){
	BERT$.Excel( 0x8000 + 98, list( Normalize.Formula( formula.or.value ), Ensure.Ref( ref ) ));
}

#--------------------------------------------------------------------
#
# get a cell or range of cells, as matrix
#
#--------------------------------------------------------------------
Get.Range <- function( ref ) {
	ref <- Ensure.Ref( ref );
	BERT$.Excel( 0x4002, list( ref ));
}

#--------------------------------------------------------------------
#
# get the selected range, as a matrix
#
#--------------------------------------------------------------------
Get.Selection <- function( ref ){ 
	sel <- BERT$.Excel( 95, list());
	Get.Range(sel);
}

#--------------------------------------------------------------------
#
# select a range of cells
#
#--------------------------------------------------------------------
Select.Range <- function( ref ) {
	ref <- Ensure.Ref( ref );
	BERT$.Excel( 0x8000 + 109, list( ref ));
}

#--------------------------------------------------------------------
#
# copy a range to the clipboard
#
#--------------------------------------------------------------------
Copy.Range <- function( ref ){
	ref <- Ensure.Ref( ref );
	BERT$.Excel( 0x8000 + 50, list( ref ))
}

#--------------------------------------------------------------------
#
# copy a range to the clipboard
#
#--------------------------------------------------------------------
Paste.Special <- function(){
	BERT$.Excel( 0x8000 + 53, list( 3, 1, FALSE, FALSE ));
}

#--------------------------------------------------------------------
#
# cancel copy
#
#--------------------------------------------------------------------
Cancel.Copy <- function(){
	BERT$.Excel( 0x8000 + 120, list());
}

#--------------------------------------------------------------------
# 
# get the current screen update state
#
#--------------------------------------------------------------------
Get.Echo <- function(){
	BERT$.Excel( 186, list( 40 ));
}

#--------------------------------------------------------------------
# 
# set the screen update state.  this function is surprisingly
# slow, so only call it for long operations.
#
#--------------------------------------------------------------------
Set.Echo <- function( new.state ){
	# BERT$.Excel( 0x8000 + 141, list( new.state ));
	BERT$.Excel( 87, list( new.state ));
}

#--------------------------------------------------------------------
#
# alternate method for setting a range of cells (to different
# values).  should be faster, but a bit hacky, and resets the
# selection.
#
# this is now the default implementation for Set.Range.
#
#--------------------------------------------------------------------
Set.Range <- function( ref, formula.or.value, default.value="", by.row=TRUE ){
	
	len <- length( formula.or.value );

	# case 1: single value (or formula)
	if( len == 1 ){
		Set.Cell( ref, formula.or.value );
	}
	else {
		
		# setting/unsetting echo seems to be counterproductive
		# cached.value <- Get.Echo();
		# Set.Echo( F );
	
		ref <- Ensure.Ref( ref );
		
		ref.row <- nrow(ref);
		ref.col <- ncol(ref);
		
		v.row = nrow( formula.or.value );
		v.col = ncol( formula.or.value );
				
		# case 2: shaped. paste in as-is.  ignore by.row for this case.
		if( !is.null( v.row )){
			
			# add default values if the data is smaller than the target range.
			# otherwise we'll get #NAs padding the range.
			if( ref.row > v.row || ref.col > v.col ){
			
				#Set.Cell( ref, default.value );
				BERT$.Excel( 0x8000 + 97, list( Normalize.Formula( default.value ), ref ));
				
				if( ref.row > v.row ) ref@R2 <- ref@R1 + v.row - 1;
				if( ref.col > v.col ) ref@C2 <- ref@C1 + v.col - 1;
			}
		}
		
		# case 3: list/vector. convert to a matrix before paste.
		else {
			formula.or.value <- matrix( formula.or.value, nrow=ref.row, ncol=ref.col, byrow=by.row );
		}
		
		# we're inlining these to avoid redundant type checking.
		
		#Set.Array( ref, formula.or.value );
		BERT$.Excel( 0x8000 + 98, list( Normalize.Formula( formula.or.value ), ref ));
		
		#Copy.Range( ref );
		BERT$.Excel( 0x8000 + 50, list( ref ));
		
		#Select.Range( ref );
		BERT$.Excel( 0x8000 + 109, list( ref ));
		
		#Paste.Special();
		BERT$.Excel( 0x8000 + 53, list( 3, 1, FALSE, FALSE ));
		
		#Cancel.Copy();
		BERT$.Excel( 0x8000 + 120, list());

		# this is nice, but unecesary

		#target <- new("xlReference", R1=ref@R1, C1=ref@C1);
		#Select.Range( target );

		# setting/unsetting echo seems to be counterproductive
		# Set.Echo( cached.value );

	}
}

#--------------------------------------------------------------------
# 
# set the number format for a range of cells.  this function
# uses the selection, so selection will be changed.
#
# number.format is a string, as in the "custom format" in
# the excel menu -- something like "#,##0.00". it can
# handle postive/negative formats and colors.
#
#--------------------------------------------------------------------
Format.Range <- function( ref, number.format ){
	ref <- Ensure.Ref( ref );
	Select.Range( ref );
	BERT$.Excel( 0x8000 + 42, list( number.format ))
}

#--------------------------------------------------------------------
#
# recalculate
#
#--------------------------------------------------------------------
Calculate.Sheet <- function(){
	BERT$.Excel( 0x8000 + 31, list());
}

#--------------------------------------------------------------------
#
# run a vba macro, by absolute name (e.g. "ThisWorkbook.Macro1")
#
#--------------------------------------------------------------------
Run.Macro <- function( name ){
	BERT$.Excel( 0x8000 + 17, list(name));
}

#--------------------------------------------------------------------
#
# list named ranges
#
#--------------------------------------------------------------------
List.Names <- function(){

	# this is returned as a matrix because of the internal
	# excel type; here we flatten for convenience
	
	unlist(BERT$.Excel( 122, list()));
}

#--------------------------------------------------------------------
#
# define a named range
#
#--------------------------------------------------------------------
Define.Name <- function( name, ref ){

	ref <- Ensure.Ref( ref );
	BERT$.Excel( 0x8000+61, list(name, ref));
	
}

#--------------------------------------------------------------------
#
# delete a name (not the range in the spreadsheet, just the name)
#
#--------------------------------------------------------------------
Delete.Name <- function( name ){

	BERT$.Excel( 0x8000 + 110, list( name ));
	
}

#--------------------------------------------------------------------
#
# activate a worksheet, by name (brings it to the front)
#
#--------------------------------------------------------------------
Activate.Sheet <- function( name ){

	BERT$.Excel( 0x8000 + 291, list( name, name, TRUE ));

}


}); # end with

#
# now attach to add them to the console search path
#

suppressMessages(attach( BERT.Excel ));

