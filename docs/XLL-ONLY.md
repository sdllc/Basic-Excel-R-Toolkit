# Running BERT without the ribbon

BERT ships two binaries that Excel loads: `BERT64.xll`, the add-in proper,
and `BERTRibbon2x64.dll`, a COM add-in that draws the BERT tab. The tab is
optional, and this describes what you get without it, why you might want
that, and the one thing that had to change to make it work.

## Why bother

The XLL is loaded by Excel from a file path and needs no registration. The
ribbon is a COM add-in: it has to be registered with `regsvr32`, it appears
in Excel's COM add-ins list where Excel can disable it after a crash, and
it is the only part of the install that touches anything outside BERT's own
directory. In a locked-down environment, or when you simply want fewer
moving parts, dropping it leaves a smaller install with the same
capabilities.

## What you lose

The tab itself, and with it the console button, and any buttons your R code
adds with `BERT$AddUserButton` -- those are ribbon controls, so there is
nowhere to draw them. `AddUserButton` still returns an id and BERT holds the
button, so if the ribbon is installed later the buttons appear.

That is the whole list. Functions in cells, the console, graphics, Excel
references, the `EXCEL` object in R and function help all work.

## Opening the console, and getting it back

The console button lives on the ribbon, so without the tab you need another
way in. There are four; the first two cover most of it.

**At startup.** Set this in `bert-config.json`:

```json
"BERT": {
  "openConsole": true
}
```

and the console opens with Excel. `EXCEL.EXE /x:BERT` does the same for a
single run.

**CONTROL+SHIFT+R.** BERT binds that to the console command at load, with
ON.KEY. It does not use `xlfRegister`'s shortcut field, which looks
inviting and does not work: the key fires, but Excel then resolves the
command in workbook scope and says "cannot run the macro
'!BERT.Console'". ON.KEY runs the name through the same path as
`ExecuteExcel4Macro`, which is how the ribbon button reaches it.

**By name, from VBA.** `BERT.Console` is a registered Excel command:

```vba
Application.Run "BERT.Console"
```

from the Immediate window, from a macro, or from a shape on a sheet with a
macro assigned. `Application.ExecuteExcel4Macro "BERT.Console()"` does the
same thing and is what the ribbon button uses.

Typing the name into the macro dialog (ALT+F8) does **not** work. That
dialog resolves what you type in workbook scope -- it runs `!BERT.Console`
-- and an XLL command is not registered in any workbook, so Excel answers
"cannot run the macro". The C API documentation's line about command names
being usable "anywhere a valid command name is required" is about XLM
contexts, not that dialog.

A shortcut of your own, if the registered one does not suit, is three lines
in `PERSONAL.XLSB`:

```vba
Sub ShowBertConsole()
    Application.Run "BERT.Console"
End Sub

' in ThisWorkbook, to bind it at startup
Private Sub Workbook_Open()
    Application.OnKey "^+R", "ShowBertConsole"
End Sub
```

**From R**, which suits a workbook that should open the console itself:

```r
ShowConsole <- function() EXCEL$Application$Run("BERT.Console")
```

### Closing it is not closing it

Clicking the X on the console hides the window. The process stays running
with R attached, so any of the routes above brings it back with the shell
history and open files intact. If the process really has gone -- killed from
Task Manager, say -- `BERT.Console` starts a new one.

Verified with the ribbon disabled: the console opened with Excel, closing it
with the X left the window hidden and the process alive, and running
`BERT.Console` made it visible again. CONTROL+SHIFT+R was tested by hand,
which is the only way to test a keystroke here.

## Turning the ribbon off on an install you already have

Excel: **File > Options > Add-ins**, set *Manage* to **COM Add-ins**, click
**Go**, and clear **BERT2 Ribbon Menu**. That leaves it registered but not
loaded, and Excel remembers the choice. The same switch lives in the
registry at
`HKCU\Software\Microsoft\Office\Excel\Addins\BERT2Ribbon.Connect`, where
`LoadBehavior` is 3 for loaded and 0 for off.

That is enough on its own: the installer registers `BERT64.xll` in Excel's
add-in list whichever way you left the components page, so the add-in goes
on loading with the ribbon switched off.

It was not always so. Before r17 -- and in the first build of r17 itself --
the ribbon was the only thing that loaded the xll, and clearing that box
left you with no BERT at all: no functions, no console, and nothing for a
keyboard shortcut to reach. On an install like that, add the xll by hand
once: File > Options > Add-ins, *Manage* **Excel Add-ins**, Go, Browse, and
pick `BERT64.xll` from the install directory.

## Installing this way

The installer's components page has a **BERT tab on the ribbon** box,
selected by default. Clear it for an XLL-only install; clear it on a later
run over an existing install and the ribbon is unregistered and removed.
For scripted installs, `/NO-RIBBON` and `/RIBBON` set it from the command
line, alongside the existing `/HELP-FEATURE` and `/NO-HELP-FEATURE`:

```
BERT-Installer-2.4.3-rNN-x64.exe /S /NO-RIBBON
```

## What had to change

Drawing into a sheet and the `EXCEL` object in R both need Excel's
`Application` object, and the add-in had no way of its own to get one: the
ribbon passed it in as it loaded, through `BERT_SetPointers`. Without the
ribbon the pointer stayed null, `BERTGraphics::CreateDeviceTarget` took its
`if (application_pointer)` branch, and plotting from a cell quietly drew
nothing at all.

The add-in now asks Excel for the pointer itself when nobody has handed one
over: find this process's Excel frame window (`XLMAIN`, matched against
`xlGetHwnd`), walk down to the sheet pane (`XLDESK` then `EXCEL7`), and call
`AccessibleObjectFromWindow` on it with `OBJID_NATIVEOM`. That returns the
window's object model, and `Application` hangs off it. This is the standard
route for an add-in with no COM component of its own; Excel-DNA does the
same thing.

It is acquired lazily, on first use rather than at load, because the pane
window does not exist while Excel is still starting up. With the ribbon
installed nothing changes: it hands the pointer over first and the fallback
never runs.
