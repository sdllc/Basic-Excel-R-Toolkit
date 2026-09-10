# Installing BERT from the zip

The installer (`BERT-Installer-<version>-x64.exe`) is the easy route: it
installs over an existing BERT 2 without touching your settings. This zip is
for anyone who prefers to copy files by hand, or has no existing BERT.

You need 64-bit Excel and a 64-bit R, 4.2 or later, from
https://cran.r-project.org.

## Over an existing BERT 2 installation

BERT 2 lives in `%LOCALAPPDATA%\BERT2` (type that into the Explorer address
bar). Close Excel, then:

1. Delete the `console`, `module` and `startup` folders there and copy the
   ones from this zip in their place.
2. Copy these files over the existing ones: `BERT64.xll`,
   `BERTRibbon2x64.dll`, `ControlR.exe`, `Welcome.md`,
   `bert-config-template.json`, `user-stylesheet-template.less`,
   `BERT-IntelliSense.xlam`. If an earlier build left `libprotobuf.dll` and
   `abseil_dll.dll` there, delete them; they are no longer used.
3. Do not copy `bert-languages.json` over yours unless you want the copy in
   this zip, which has no R location in it.
4. Tell BERT where your R is, in `bert-config.json` (the installer would
   have done this for you):

   ```json
   "R": {
     "home": "C:\\Program Files\\R\\R-4.5.2"
   }
   ```

Your `bert-config.json`, `user-stylesheet.less` and the files in
`Documents\BERT2` are untouched.

## Function help in the formula bar (optional)

The installer offers this as a tick box; from the zip it is manual. Copy
`ExcelDna.IntelliSense64.xll`, `ExcelDna.IntelliSense-License.txt` and
`BERT-IntelliSense.xlam` into the BERT directory as well, then load the two
add-ins through File > Options > Add-ins > Manage Excel Add-ins > Browse.
See `docs/FUNCTION-HELP.md` for what they do. Skip them if you do not want
the feature; nothing else depends on them.

## A fresh installation

1. Create `%LOCALAPPDATA%\BERT2` and copy everything from the zip into it.
2. Copy `bert-config-template.json` to `bert-config.json` and set the R
   location in it as above.
3. Register the ribbon add-in, from a command prompt:

   ```
   regsvr32 "%LOCALAPPDATA%\BERT2\BERTRibbon2x64.dll"
   ```

4. Start Excel. If BERT does not load, add `BERT64.xll` through File >
   Options > Add-ins > Manage Excel Add-ins > Browse.
