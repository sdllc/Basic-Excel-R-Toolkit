
CRAN Package Descriptions
-------------------------

I'm sure this is available somewhere in a usable format, I just can't find it.

We want a map of package names -> short descriptions that we can display in a
UI.  This script will pull the HTML table from a mirror and format the data as 
JSON.  

The intent is to have clients read this file via CDN.  This data should be 
fairly static, so we will update it periodically but infrequently.  When the 
data is updated the file will be checked in, so reading the latest commit 
should give the most recent data.  Currently:

https://cdn.rawgit.com/sdllc/BERTConsole/a96ccbf9eb74849d17fe2f8fa6feb18d62346ff5/util/packages.json

Thanks to [RawGit][1] and [StackPath][2].

Run
---

```
perl generate-package-table.pl
```

pass `-?` for a list of options.

License
-------

The script `generate-package-table.pl` and the generated file `packages.json` 
are placed in the public domain.

[1]: https://rawgit.com/
[2]: https://stackpath.com/