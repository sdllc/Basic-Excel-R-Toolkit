
#= 

The Excel scripting interface, via COM, is object-oriented. Julia
is not. We decided to map COM onto Julia using objects. For a variety
of reasons this seemed like the better alternative, although it 
results in a syntax that seems somewhat unjulian.

We may revisit this in the future, pending some more real-world 
experience. For the time being, here is how to use the Excel 
interface. Paste this code into the shell (or right-click the editor
and execute).

=#

#
# get the active workbook. create one if it doesn't exist
# (you're on the start screen?)
#
wb = EXCEL.Application.get_ActiveWorkbook()
if( wb == nothing )
  wb = EXCEL.Application.get_Workbooks().Add()
end

#
# get the worksheets pointer and add a new sheet
#
new_sheet = wb.get_Worksheets().Add()

#
# change the name. note this will fail if a sheet with this name 
# already exists (like if you run this code twice)
#
new_sheet.put_Name("Julia Data")

#
# create some test data -- here it's a 20x5 array
#
data = reshape(randn(100), (20,5))

#
# now insert this into the new sheet
#
new_sheet.get_Range("B2:F21").put_Value(data)

#
# add some headers. NOTE: transponse does not work on 
# strings in julia? seems like an oversight. 
# [NB: it's `permutedims`]
#
header_range = new_sheet.get_Range("B1:F1")
header_range.put_Value(hcat("A", "B", "C", "D", "E"))

#
# example of using constants: add border (underline)
#
border = header_range.get_Borders(EXCEL.XlBordersIndex.xlEdgeBottom)
border.put_Weight( EXCEL.XlBorderWeight.xlThin )

#
# center headers
#
header_range.put_HorizontalAlignment(EXCEL.Constants.xlCenter)

