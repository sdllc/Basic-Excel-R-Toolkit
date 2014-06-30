
#
# get a sheet ID by name.  sheet IDs are internal to Excel 
#
GetSheetID <- function( sheetName )
{
	ref <- BERT$Excel( 0x4004, list( sheetName )); 
	return( ref$sheetID );
}

#
# create a range from a text string (e.g. "A1"), potentially
# including the sheet name ("Sheet1!A2")
#
Text.Ref <- function( text ) {

	# assume A1 input, but check?
	
	ref <- BERT$Excel( 147, list( text, TRUE ));
	if( is.null( ref )) ref <- BERT$Excel( 147, list( text, FALSE ));
	
	# this function (xlfTextref) returns a non-usable sheet ID.
	# not sure what it actually refers to, but don't use it
	ref$sheetID = vector( mode="integer", length=2);

	mm <- regexpr( "(.*?)\\!", text )
	if( mm >= 0 )
	{
		sname <- substr( text, mm, attr( mm, "match.length") - mm );
		ref$sheetID <- GetSheetID( sname );
	}

	return( ref );
}

#
# verifies a range, or creates a range from text
#
Ensure.Ref <- function( ref ) 
{
	# ref is a string, try to convert
	if( is.character( ref )) ref <- Text.Ref( ref );

	# check ref type
	if( !inherits( ref, "xlRefClass" )) stop( "Bad ref type" );

	return( ref );
}

#
# set a cell formula (or value) 
#
Set.Cell <- function( ref, formula ) {

	ref <- Ensure.Ref( ref );	

	rc <- BERT$Excel( 241, list( formula, TRUE, FALSE, 1, BERT$xlRefClass( 1, 1 )));
	# cat( "RC:", rc, "\n" );

	# set
	BERT$Excel( 0x8000 + 96, list( rc, ref ));

}

#
# get a cell or range of cells, as matrix
#
Get.Range <- function( ref ) {

	ref <- Ensure.Ref( ref );	
	BERT$Excel( 0x4002, list( ref )); 
}

#
# select a range of cells
#
Select.Range <- function( ref ) {

	ref <- Ensure.Ref( ref );
	BERT$Excel( 0x8000 + 109, list( ref ));

}

#
# select a range of cells
#
Calculate.Sheet <- function(){

	BERT$Excel( 0x8000 + 31, list());

}

#
# run a vba macro, by absolute name (e.g. "ThisWorkbook.Macro1")
#
Run.Macro <- function( name ){

	BERT$Excel( 0x8000 + 17, list( name ));

}

