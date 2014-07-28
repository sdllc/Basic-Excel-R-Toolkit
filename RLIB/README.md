
R Libraries for Linking
=======================

These libraries were generated from the DLLs in order to support dynamically
linking the R DLL at runtime.

See, e.g., https://stat.ethz.ch/pipermail/r-devel/2010-October/058833.html:

```

I successfully did the following recently in order to build 64 bit PL/R
on Windows 7:

----------------
dumpbin /exports R.dll > R.dump.csv

Note that I used the csv extension so OpenOffice would import the file
into a spreadsheet conveniently.

Edit R.dump.csv to produce a one column file of symbols called R.def.
Add the following two lines to the top of the file:
 LIBRARY R
 EXPORTS

Then run the following using R.def

 lib /def:R.def /out:R.lib
----------------

HTH,

Joe

```
