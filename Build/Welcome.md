

# Welcome to the BERT console! # 

BERT connects Excel with the statistics language [R][1]. The console has
an editor for writing functions, and an R shell for testing code. You can 
also control Excel directly from R in the shell. See the file 
excel-scripting.R (in your Documents folder, under BERT2) for an example.

When it starts, BERT loads functions from the functions directory, in 
your Documents folder under BERT2. You should see some example functions 
installed in Excel called `R.TestAdd` and `R.EigenValues`.

You can also use BERT with the statistics language [Julia][2]. See 
[this page][3] for more information. 

Have suggestions, feedback, questions, comments?  Let us know!  

Cheers,

 -- [The BERT team][4]

[1]: https://cran.r-project.org/
[2]: https://julialang.org/downloads/
[3]: https://bert-toolkit.com/using-julia-with-bert
[4]: https://bert-toolkit.com/contact

# Release Notes #

2.0.23 -- Release Candidate (14 March 2018)

Add R to default install. Simplify configuration, add language defaults.
Julia should now work automatically if installed to the default directory.
Add `BERT.Call` and `BERT.Exec` for backwards compatibility.

2.0.21 -- Preview Release (7 March 2018)

Add Excel graphics device, bugfixes and updates. Moved `BERT.Call.X`
and `BERT.Exec.X` to macro category (limit to VBA calls).

2.0.20 -- Preview Release (3 March 2018)

Add mirror and package selection dialogs to the console. Fixes for UTF-8 
handling. Application restructuring and lots of bugfixes and updates.

2.0.19 -- Preview Release (28 February 2018)

Add 32-bit Excel support (on 64-bit Windows). Fixes for console R graphics.
Restructured language config to move out of code.

2.0.18 -- Preview Release (27 February 2018)

bugfixes and updates

2.0.15 -- Preview Release (26 February 2018)

Version 2 represents a complete rewrite and rearchitecting of BERT. In 
particular, language services are moved out of process. This allows us to
support multiple languages at once, as well as support killing and restarting
languages. We can also connect 32-bit Excel to 64-bit R, although for the 
time being we are only providing a plug in for 64-bit Excel.

Compared to mainline BERT, this version is missing the Excel graphics device
(although there is a console graphics device, via `BERT.Console.graphics.device`). 
All other functionality should be present, but there may be bugs.

Some functions are installed in Excel but will be removed in future releases.
These are the `BERT.Call.X` and `BERT.Exec.X` functions, allowing arbitrary 
function calls or code execution in various languages. These will be removed
from the spreadsheet and limited to VBA.

# Credits #

The BERT console is built using [Electron][5], [Monaco][6] and 
[xtermjs][7]. 

[5]: https://electronjs.org/
[6]: https://github.com/Microsoft/monaco-editor
[7]: https://xtermjs.org/
