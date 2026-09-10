[<img src="https://raw.githubusercontent.com/SamLovick/Basic-Excel-R-Toolkit/master/logo-transparent.svg">](https://bert-toolkit.com/)

About this fork
---------------

This is a fork of [BERT](https://github.com/sdllc/Basic-Excel-R-Toolkit) by
Structured Data, carried forward to current R and a current toolchain. The
upstream project has not released since 2018, and its 2.4.4 build is tied to
the R 3.5 it ships with.

What is different here: one controller built against R 4.5.2, with the R
version checked at runtime rather than compiled in, so it runs against the R
you already have, from 3.5 up to 4.6. R is no longer bundled, and the
installer finds your existing installation. Graphics is the one part tied to
a particular R: `BERTModule` is compiled against R's graphics engine, whose
version changes between R series, so the install ships one module per series
and uses the one that matches. **Modules ship for 3.5, 4.2, 4.3, 4.4, 4.5
and 4.6** -- the whole range the controller runs on -- so drawing works
wherever the rest does. Strings are UTF-8 from end to end. The build is x64
only, on the v145 toolset with protobuf 5.29, linked statically so it needs
no Visual C++ redistributable. The console runs on current Electron, Monaco
and xterm. Your own functions can carry argument help, shown in Excel's
function dialogs and in the formula bar -- see [function help][r5]. Two
long-standing faults are fixed along the way: a deadlock whenever an R
function called back into Excel, which made the graphics device unusable,
and a console that dropped any reply larger than 64k.

### The ribbon tab is optional

BERT puts two things into Excel: the add-in proper, `BERT64.xll`, and a COM
add-in that draws the BERT tab. The tab is a component on the installer's
page, selected by default. Clear it -- or pass `/NO-RIBBON` to a silent
install -- and you get an add-in that registers nothing with COM and has no
entry in Excel's COM add-ins list for Excel to disable after a crash.

Everything except the tab still works: functions in cells, the console,
graphics, Excel references, the `EXCEL` object in R and function help. What
goes with it is the tab itself and any buttons your R code adds with
`BERT$AddUserButton`. The console button lives on the tab, so without it
press CONTROL+SHIFT+R, or set `"openConsole": true` in `bert-config.json` to
have the console open with Excel. See [running without the ribbon][r6].

[Releases][r1] &middot; [Changelog][r2] &middot; [Building][r3]. The Julia
controllers are untouched and unbuilt here; this work is about R. The whole
change set is offered back to the upstream project as [pull request #220][r4].

[r1]: https://github.com/SamLovick/Basic-Excel-R-Toolkit/releases/latest
[r2]: https://github.com/SamLovick/Basic-Excel-R-Toolkit/blob/master/CHANGELOG.md
[r3]: https://github.com/SamLovick/Basic-Excel-R-Toolkit/blob/master/docs/BUILDING.md
[r4]: https://github.com/sdllc/Basic-Excel-R-Toolkit/pull/220
[r5]: https://github.com/SamLovick/Basic-Excel-R-Toolkit/blob/master/docs/FUNCTION-HELP.md
[r6]: https://github.com/SamLovick/Basic-Excel-R-Toolkit/blob/master/docs/XLL-ONLY.md

---

The most up-to-date documentation for BERT is on the website (https://bert-toolkit.com):

 * [Quick Start][1]
 * [Example Functions][2]
 * [Talking to Excel from R][3]
 * [The Excel Scripting Interface][4]

To install BERT, download the [latest release][5].

[1]: https://bert-toolkit.com/bert-quick-start
[2]: https://bert-toolkit.com/bert-example-functions
[3]: https://bert-toolkit.com/talking-to-excel-from-r
[4]: https://bert-toolkit.com/excel-scripting-interface-in-r
[5]: https://github.com/SamLovick/Basic-Excel-R-Toolkit/releases/latest

Overview
--------

BERT is a connector for Excel and the programming languages R and Julia. 
Put some R functions in a file; open Excel, and use those functions in your 
spreadsheets. Essentially anything you can do in R or Julia, you can call 
from an Excel spreadsheet cell. 

There's also a console for talking to Excel from these programming languages, 
and (if you want) you can run R or Julia code from VBA as well.

Verion 2
--------

The new version of BERT moves R out of process for better stability, code
separation, and future feature development (abortable/restartable code service,
additional languages). We're also rewriting a lot of stuff just to remove cruft
and use more modern C++.

The new version uses a monorepo so we can tie together the various components,
instead of having mutliple repos.  Once this is the active version we will 
shut down the separate components.

Roadmap
-------

 * Full replacement for BERTv1

   Most of BERT has been rewritten from scratch for the new version. The result
   is a more stable and extensible base, with better structure and generally 
   cleaner and more consistent code.

 * Console rewrite

   The console has also been rewritten in typescript, which is a better 
   foundation for what is now a fairly large project. 

 * Additional language(s)

   Separation between the interface (Excel) and the language services means
   we can support more than one language. BERT currently supports R and Julia,
   and we can add more languages in the future.

Requirements (Runtime)
----------------------

 * Excel

   64-bit Excel: Microsoft 365, or 2016 and later, on Windows 10 1903 or
   later. This fork builds x64 only; the 32-bit add-in is no longer built
   or shipped.

 * R

   BERT does not include R, so you will need an R installation. A plain-
   vanilla [Windows R install][6] is fine. 4.2 or later is recommended, and
   is what the installer looks for; the controller will start against
   anything from 3.5 up.

 * Julia 0.6.2 (optional)

   The same applies to Julia; if you want to integrate Julia, use a plain-
   vanilla [Windows install of Julia][7]. You must use the current release
   (0.6.2); When Julia releases 0.7, we will update to match.

Requirements (Building)
-----------------------

The notes below describe the original 2018 setup. For the current build,
including how the R version is chosen, see [docs/BUILDING.md][r3].

There are several third party tools and libraries used to build BERT:

 * Protocol Buffers

   BERT uses [Protocol Buffers][8] for IPC. This requires the protoc compiler
   (to compile .proto files) as well as runtime libraries. This fork uses
   version 5.29 with the proto3 syntax, installed through vcpkg in manifest
   mode, so the build fetches and builds it for you.

 * Excel SDK

   The [Excel SDK][9] provides XLCALL.cpp and XLCALL.h for Excel integration.

 * R, including headers and .libs

   To build R components, you will need R. A standard R distribution includes 
   headers and DLLs, but you need to build libs for linking. For tips on how 
   to do this, see (e.g.) [this mailing list post][10].

 * Julia

   A plain-vanilla Windows install of Julia is sufficient.

 * Node and npm

   Building the console requires a recent version of [node][11] and npm, plus
   the libraries specified in `dependencies` and `devDependencies`. This fork
   builds with npm and a checked-in `package-lock.json`; the original used
   [yarn][12].

License
-------

BERT is provided under the [GPL (v3)][13]. Contact us for alternate licensing
options.

[6]: https://cran.r-project.org/bin/windows/base/
[7]: https://julialang.org/

[8]: https://developers.google.com/protocol-buffers/
[9]: https://msdn.microsoft.com/en-us/library/office/bb687883.aspx
[10]: https://stat.ethz.ch/pipermail/r-devel/2010-October/058833.html
[11]: https://nodejs.org
[12]: https://yarnpkg.com
[13]: https://www.gnu.org/licenses/gpl-3.0.md