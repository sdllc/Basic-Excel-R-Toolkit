
BERT2
=====

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

  The main aim of the v2 project is to fully rewrite BERT, with a more stable
  and extensible base. The original BERT project was kind of a mishmash of old
  code and libraries, plus new stuff written on the fly. v2 gives us the 
  opportunity to go back and clean up, organize, and streamline how it works.

* Additional language(s)

  Separation between the interface (Excel) and the language service (R) means
  we should be able to add more languages without too much trouble. While R is 
  still the main target, we will look to add at least one additional language
  as an experiment (possibly Julia, maybe Javascript).

* LibreOffice implementation

  The same applies to the front-end; so we'll look at implementing at least 
  basic support in LibreOffice (assuming it's feasible).

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
