BERT Ribbon 2
=============

Switching from the VBA-based ribbon to a dll ribbon.  The VBA
ribbon works fine.  The problem is that it pollutes the VBA
workspace with its own files.  This is just unecessary.  An
ATL-based ribbon is a bit more complicated, but ultimately
it's a better solution.  It also won't get caught up in any
future VBA lockdowns.

The ribbon is going to become the default interface, and the
menu will be secondary.  ~~As of this writing, there's a registry
entry to *hide* the menu; that's going to turn into a registry
entry to *show* the menu.~~  On second thought, we'll leave
the menu operational absent the registry entry; that makes it
easier to build-and-run, and installers are setting registry
entries anyway.

Building
--------

If you are building from source, the ribbon menu has its own
registration script which will set up Excel.  Build the 32 or
64 bit release (depending on your **Excel** bitness), then
from a shell call

```
Regsvr32 /i:user BERTRibbon2x64.dll
```

or

```
Regsvr32 /i:user BERTRibbon2x86.dll
```

To remove it, add `/u` to that command.
