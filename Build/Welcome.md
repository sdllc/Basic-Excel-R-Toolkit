

# Welcome to the BERT console! # 

This is a preview release of BERT version 2. To get started, you 
will need to configure BERT to connect to R or Julia. This release
does not include either language.

**BERT requires specific versions of each language**. For R, you can use 
the current release (3.4.3) or any 3.4.x version. For Julia, you 
must use the current release (0.6.2). You can use an existing install or
download default installers for [R][1] or [Julia][2].

Open the preferences file using the menu **View** > **Preferences**. For 
either R or Julia (or both), set "home" to point to the language root 
directory.

After you set the directory, you will need to restart Excel for 
the changes to take effect. Re-open the console and you should
see a tab for your installed language or languages. BERT loads functions
from the functions directory; this is installed in your Documents folder
under BERT2. You should see some example functions installed in Excel
called `R.TestAdd` and `R.EigenValues` (or `Jl.TestAdd` and `Jl.EigenValues`).

Have suggestions, feedback, questions, comments?  Let us know!  

Cheers,

 -- [The BERT team][3]

[1]: https://cran.r-project.org/
[2]: https://julialang.org/downloads/
[3]: https://bert-toolkit.com/contact

# Release Notes #

2.0.17 -- preview release

bugfixes and updates

2.0.15 -- preview release

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

The BERT console is built using [Electron][4], [Monaco][5] and 
[xtermjs][6]. 

[4]: https://electronjs.org/
[5]: https://github.com/Microsoft/monaco-editor
[6]: https://xtermjs.org/

