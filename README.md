
Basic Excel R Toolkit (BERT)
============================

Please note: user documentation is moving to [the wiki] [1].  We'll
leave this file here, and may update it from time to time, but if 
there's a conflict between the two the wiki should be presumed to be
correct.


Overview
--------

BERT is a simple connector for Excel and R.  Put some R functions in a file;
open Excel, and use those functions in your spreadsheets.  Essentially
anything you can do in R, you can call from an Excel spreadsheet cell.
There's also a rudimentary (working on that) console for debugging.  You can,
if necessary, execute arbitrary R code from VBA as well.

Usage
-----

The binary installer includes some simple example functions.

 1. Run the installer

 2. Open a new spreadsheet

 3. Select a cell and type `=R.Fibonacci(10)`

That's pretty much it.  Next step is to add your own functions:

 1. From the **Add-ins** tab in Excel, click the **BERT** menu and
    **Home Directory**.

 2. Add your code to the `Functions.R` file and save it

 3. From the **Add-ins** tab again, click the **BERT** menu and
    **Reload Startup File**.  Your functions should now be available in Excel.
    If there's been an error in reading your file, it will show up in Excel's
    status bar.

Construction
------------

BERT is an XLL (Excel extension library).  On startup, it reads the given
source file and looks for functions.  It uses its own R installation, so it
won't affect any other workspaces you have on your machine.

By default it picks out functions from the global environment (via `lsf.str`);
you can use a different environment if you want, but that environment must
be exposed from the global environment (we will locate it with `get`).

For each function, it creates an Excel function with the same arguments
(which we determine via `formals`).  When you type in the function, or
recalculate, the Add-in executes the function using `do.call`.

BERT uses the Excel C API, so it's fast.  It also exposes that API to you,
the developer, so it's powerful (but can be dangerous).

Arguments and Return Types
--------------------------

Single-value arguments are passed to functions as basic types (Real, Integer,
String or Boolean) depending on what Excel types they are.  If you pass a range
of cells as an argument, that will get passed to the R function as a matrix.

If all cells in the range are numeric, the matrix will be passed as a Real matrix.
If any cell is not numeric (including Nil and Boolean) a generic matrix will
be used instead.

Return values work the same way; single values are mapped directly to types,
and any result with length > 1 is returned as an Excel Array (see below).

Workspace and R Installation
----------------------------

BERT uses its own R installation and its own workspace.  It's designed to not
affect or interact with any other R installation on your machine.  That does
mean you will need to separately install any modules you need, although you
only have to do that once.

#### Can I use my existing R installation? ####

It is possible, although we don't recommend it.  Have a look at the sources
and figure out how to do that.  Also if you use a different version of R than
the one used to build it (currently 3.1.0), it will break.

If you just want to reuse existing R code, you can `source` it from the BERT
startup file.

Performance
-----------

There is some overhead in Excel -> R calls, in part because there are
several layers of abstraction, and in part because there's a dynamic element
to the call.  However performance is still pretty good.

Mapping the simplest possible function `NOOP <- function(a, b){ 0 }`
we find that over 1,000,000 function calls the average time for one call
is 42.2 µs.  The Excel native function `SUM` over 1,000,000 calls takes
on average 5.445 µs/call.  So R functions are an order of magintude slower.

However this is fixed overhead, and beyond this the calculation time is
entirely dependent on R performance.  For complex functions, 42 µs is
effectively noise.  (If you actually have 1 million function calls in an
Excel spreadsheet, you may be doing it wrong.  Also that still only takes 42
seconds).

More importantly for performance, BERT is not threaded (see below).

Excel Arrays
------------

If a function returns a vector of length > 1 (or a matrix, data frame, list, or
anything other than a single scalar), the return value will be the Excel Array
type.  Excel Arrays can be entered into a spreadsheet directly, although you
need to know the size of the Array beforehand.  

1. Select a 4x4 matrix in the spreadsheet

2. Type the function `=R.Identity(4)` in the function bar

3. Press `Ctrl+Shift+Return`

This will enter the Array into the selected cells.  You can spot Arrays by
looking at the function bar; here you'll see `{=R.Identity(4)}`.  You can
change the shape of the array, but it's a pain.

The other thing you can do with Excel Arrays is pass them into functions
which can handle them.  For example, the function `=SUM( R.Identity(4))`.

Data frames are returned as Excel Arrays including their row and column
headers.  That makes them better for building spreadsheet reports, but
less useful when passing into functions.  If you intend to pass a data
frame into a function, your R code should convert the data (less headers) into
a matrix.


Limitations / Issues
--------------------

#### (Not) Threading ####

BERT is single-threaded, which means it can't take advantage of Excel's
multithreaded calculation.  Depending on the particular spreadsheet, this can
be a real drawback.  We may address this in the future.

#### No Side-Effects ####

BERT is designed for atomic functions.  You should not use global variables
or rely on data outside of your function itself (although theoretically, static
data outside of the function should be OK).

The workspace may be destroyed and re-created at any time.  There is no
guarantee of ordering of function calls.  And in particular if we implement
threading, then there may be multiple workspaces at any one time.

On a related note, the workspace is not saved when Excel closes.  Any data or
libraries you need should be explicitly loaded in your startup file.

#### Case-Sensitivity ####

R is case-sensitive, but Excel is not.  If you have two functions with the
same name with different casing, one will get overwritten.

#### Function Limit ####

Currently BERT is limited to 100 functions.  This is essentially arbitrary, and
can be increased.

#### Argument Limit ####

Currently BERT functions have a maximum 16 arguments functions.  This is
arbitrary but there is a hard Excel limit of 32 (?) arguments.  Although sloppy,
if you need more inputs you can usually accomplish this using ranges (matrices).

Compatibility
-------------

BERT is compatible with Windows Excel 2007, 2010 and 2013, both 32- and 64-bit.  
There is no support for Excel 2003 or Macintosh.

Building From Source
--------------------

BERT was built with Visual Studio 2013.  We have not tested other compilers, although
there is no magic in here.  We do use some (non-standard) Microsoft string functions.

#### Build Dependencies ####

 * [Microsoft Excel Developer's Toolkit Version 15.0] [2].

The project expects the files `XLCALL.h` and `XLCALL.cpp` in the ExcelLib
directory; these are not included in our source distribution for license
reasons.

 * Standard R binary install ("base").  R sources are not required.  

The project include directories should point to the `include/` directory in the
R base distribution.  Either the `bin/i386` or `bin/x64` directory should be in
the user PATH, depending on your Excel bitness (not Windows bitness).

#### Running / Debugging ####

BERT needs a binary R base directory, as well as a home directory, and a startup
script (in that home directory).  Defaults are `%APPDATA%\BERT` and can be
changed via the registry (see `BERT.h`).

There's an additional registry value, bitness, which controls which DLL gets
loaded at runtime.  This should be either 32 or 64 and match the bitness of
your Excel installation (not your Windows installation).

Default values:

    Home directory:  %APPDATA%\BERT
    R Bin directory: %APPDATA%\BERT\R-3.1.0
    Startup file:    Functions.R
    Bitness:         32


[1]: https://github.com/StructuredDataLLC/Basic-Excel-R-Toolkit/wiki

[2]: http://msdn.microsoft.com/en-us/library/office/bb687883(v=office.15).aspx
