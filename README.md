
BERT2
=====

The new version of BERT moves R out of process for better stability, code
separation, and future feature development (abortable/restartable code service,
additional languages). We're also rewriting a lot of stuff just to remove cruft
and use more modern C++.

The new version uses a monorepo so we can tie together the various components,
instead of having mutliple repos.  Once this is the active version we will 
shut down the separate components.

Requirements
------------

* Protocol Buffers

  BERT uses [Protocol Buffers][1] for IPC. This requires the protoc compiler
  (to compile .proto files) as well as runtime libraries, which must be built 
  by compiling the protobuf library. We're currently using version 3.5.0 and 
  the version 3 syntax.

* Excel SDK

  The [Excel SDK][2] provides XLCALL.cpp and XLCALL.h for Excel integration.

* R, including headers and .libs

  A standard R distribution includes headers and DLLs, but you need to build
  libs for linking. For tips on how to do this, see (e.g.) [this mailing list 
  post][3].

[1]: https://developers.google.com/protocol-buffers/
[2]: https://msdn.microsoft.com/en-us/library/office/bb687883.aspx
[3]: https://stat.ethz.ch/pipermail/r-devel/2010-October/058833.html
