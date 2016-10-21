
#
# Here's an example of using the Excel scripting interface
# (paste this code into the shell). 
# 
# For more information, see
#
# https://bert-toolkit.com/excel-scripting-interface-in-r
#

#
# get a reference to the workbook
#
wb <- EXCEL$Application$get_ActiveWorkbook();

#
# add a sheet
#
new.sheet <- wb$get_Sheets()$Add();

# 
# change the name of the new sheet.  note that this will
# fail if a sheet with this name already exists. 
#
new.sheet$put_Name( "R Data Set" );

#
# add some data.  use matrices.
#
range <- new.sheet$get_Range( "B3:F152" );
range$put_Value( as.matrix( iris ));

#
# add column headers
#
header.range <- new.sheet$get_Range( "B2:F2" );
header.range$put_Value( t( colnames( iris )));
