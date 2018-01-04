
BERTModule
==========

Support functions for [BERT][1].

Overview
--------

This module is part of BERT, and probably not useful on its own.

Some BERT functionality (primarily R, but some C++ as well) is 
moving out of the core and into a separate module.  This is 
happening for two reasons: (1) proper namespacing; (2) proper 
documentation for functions; and (3) the graphics device.  

Regarding the last one, building the module allows better interfacing
with R as BERT is built using MSVC while R on Windows is (usually) 
built with [mingw][3].

License 
-------

[GPL v2][2].  Copyright (c) 2017 Structured Data, LLC.

Build and install
-----------------

Set appropriate environment variables (see generally docs on
building modules; you will need Windows R build tools installed). 
To generate package and documentation files,

```
> R -e "library(roxygen2); roxygenise('BERTModule');"
```

Build with

```
> R CMD build BERTModule && R CMD INSTALL BERTModule
```

[1]: https://bert-toolkit.com
[2]: https://opensource.org/licenses/gpl-2.0.php
[3]: http://www.mingw.org/

