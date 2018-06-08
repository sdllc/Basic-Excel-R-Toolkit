

# Welcome to the BERT console! # 

The console has an editor for writing functions (this side), and an R shell 
for testing code. You can also control Excel directly from R in the shell; 
see `excel-scripting.R` (in your Documents folder, under BERT2) for 
an example.

When it starts, BERT loads functions from the functions directory, 
Documents\BERT2. You should see some example functions installed in Excel 
called `R.TestAdd` and `R.EigenValues`.

You can also use BERT with the statistics language Julia. See 
[this page][3] for more information. 

Have suggestions, feedback, questions, comments?  Let us know!  

Cheers,

 -- [The BERT team][4]

[1]: https://cran.r-project.org/
[2]: https://julialang.org/downloads/
[3]: https://bert-toolkit.com/using-julia-with-bert
[4]: https://bert-toolkit.com/contact

# Upgrading from BERT v1? #

If you are upgrading from an older version of BERT, [check our website][5] for
some additional information. 

[5]: https://bert-toolkit.com/whats-new#upgrading-from-bert-1

# Release Notes #

2.4.0 -- Update for R 3.5.0 and support Julia 0.6.3.

2.3.0 -- Add support for Julia 0.7. [See our website for details][11].
Add versioning info to R, Julia. Support horizontal scrolling in R.

2.2.1 -- Fix for Windows 7 x64

2.2.0 -- Update for R 3.4.4

Updates R to 3.4.4. Minor updates and bug fixes.

2.1.0 -- First Release of BERT 2 (15 March 2018)

Version 2 represents a complete rewrite and rearchitecting of BERT. In 
particular, language services are moved out of process. This allows us to
support multiple languages at once, as well as support killing and restarting
language services. We can also connect 32-bit Excel to 64-bit R.

Version 2 includes support for R and Julia. In keeping with earlier BERT 
releases, we include R in the default install, but you will need to install
Julia separately to use it.

[11]: https://bert-toolkit.com/using-julia-with-bert#julia-07

# Credits #

The BERT console is built using [Electron][20], [Monaco][21] and 
[xtermjs][22]. 

[20]: https://electronjs.org/
[21]: https://github.com/Microsoft/monaco-editor
[22]: https://xtermjs.org/
