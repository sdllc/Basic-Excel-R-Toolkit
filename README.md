[<img src="logo-transparent.svg">](https://bert-toolkit.com/)

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
[5]: https://github.com/sdllc/Basic-Excel-R-Toolkit/releases/latest

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

   BERT supports Excel 2010, 2013 and 2016, both 32-bit and 64-bit (but 
   only on 64-bit Windows).

 * R 3.4.x (optional)
  
   This version of BERT does not (at the moment) include R, so you will need
   an R installation. A plain-vanilla [Windows R install][6] is fine, as long
   as it is version 3.4.0 or later.

 * Julia 0.6.2 (optional)

   The same applies to Julia; if you want to integrate Julia, use a plain-
   vanilla [Windows install of Julia][7]. You must use the current release
   (0.6.2); When Julia releases 0.7, we will update to match.

Requirements (Building)
-----------------------

There are several third party tools and libraries used to build BERT:

 * Protocol Buffers

   BERT uses [Protocol Buffers][8] for IPC. This requires the protoc compiler
   (to compile .proto files) as well as runtime libraries, which must be
   built by compiling the protobuf library. We're currently using version 
   3.5.0 and the version 3 syntax.

 * Excel SDK

   The [Excel SDK][9] provides XLCALL.cpp and XLCALL.h for Excel integration.

 * R, including headers and .libs

   To build R components, you will need R. A standard R distribution includes 
   headers and DLLs, but you need to build libs for linking. For tips on how 
   to do this, see (e.g.) [this mailing list post][10].

 * Julia

   A plain-vanilla Windows install of Julia is sufficient.

 * Node and Yarn (or npm)

   Building the console requires a recent version of [node][11] and [yarn][12] 
   (or npm), plus the libraries specified in `dependencies` and 
   `devDependencies`.

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