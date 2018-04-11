
#
# Here's an example of using the Excel scripting interface
# (paste this code into the shell). 
# 
# For more information, see
#
# https://bert-toolkit.com/excel-scripting-interface-in-r
#

#
# get a reference to the workbook. create a new one if 
# it doesn't exist (you're on the start screen?)
#
wb <- EXCEL$Application$get_ActiveWorkbook();
if(is.null(wb)){
  wb <- EXCEL$Application$get_Workbooks()$Add();
}

#
# add a sheet
#
new.sheet <- wb$get_Sheets()$Add();

# 
# change the name of the new sheet. note that this will
# fail if a sheet with this name already exists. 
#
new.sheet$put_Name( "R Data Set" );

#
# add some data. includes headers
#
range <- new.sheet$get_Range( "B2:F152" );
range$put_Value( iris );

#
# add column headers
#
header.range <- new.sheet$get_Range( "B2:F2" );
#header.range$put_Value( t( colnames( iris )));

# 
# resize columns to fit
#
range$get_EntireColumn()$AutoFit();

#
# example of using constants: add border (underline) to headers
#
border <- header.range$get_Borders(EXCEL$XlBordersIndex$xlEdgeBottom);
border$put_Weight( EXCEL$XlBorderWeight$xlThin );

#
# center headers
#
header.range$put_HorizontalAlignment(EXCEL$Constants$xlCenter);

