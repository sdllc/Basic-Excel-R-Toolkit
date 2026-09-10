# Modernisation roadmap

This is the plan for bringing BERT up to date after the `r4-support` branch,
which makes the R controller work with R 4.x. It records what changed in R
and in the toolchain since the 2018 release, what has been done about each
change, and what is still to do, in a suggested order.

## Where the tree stands

* Native projects build with the v145 toolset in Visual Studio 2026, take
  protobuf 5.29.5 from vcpkg in manifest mode, and have ARM64EC
  configurations alongside x64.
* `ControlR.exe` is a single binary built against R 4.5.2. It checks the R
  version at startup instead of demanding exactly 3.5, and switches console
  output handling on the 4.2.0 change of native encoding. It starts under
  R 3.5.0, 4.2.2 and 4.5.2. Only 4.5.2 has been used with Excel.
* The console (on the `console-upgrade` branch) runs on Electron 44,
  TypeScript 7, Monaco 0.56, xterm 6, rxjs 7 and google-protobuf 4, with
  the smaller libraries current too. It keeps its architecture: Node in
  the renderer, `@electron/remote` for the few main-process calls. It has
  been used inside Excel: pipes connect, the editor and the R shell work.
  Left over from that work: the code is compiled with strict mode off
  and would benefit from being made strict-clean; Monaco is still loaded
  through its AMD loader, which in an Electron renderer uses Node's `vm`
  module (Electron warns about it), and the way out is bundling Monaco as
  ES modules; the page has no content security policy; the protobuf
  JavaScript is generated code from the 3.21 generator running on the 4.0
  runtime, and can be regenerated with protoc-gen-js 4.0.
* The Julia controllers target Julia 0.6 and 0.7 and are untouched. They
  cannot host Julia 1.x.
* The installer still bundles R 3.5.0.
* There are no automated tests, and nothing runs in CI.

## What changed in R since 3.5, and what it means here

| R | Change | Effect on BERT |
| --- | --- | --- |
| 4.0.0 | `stringsAsFactors` defaults to `FALSE` | R-level code in `Module/` and `Build/startup/startup.R` should be audited for reliance on the old default |
| 4.1.0 | Graphics engine 14: `DevDesc` gains pattern, clip-path and mask callbacks and a `deviceVersion` field | A device that leaves `deviceVersion` at 0 is treated as pre-4.1 and the new callbacks are never called. The two BERT devices do not set it, so this depends on the descriptor being zero-initialised. Set it explicitly (see below) |
| 4.2.0 | Native encoding becomes UTF-8 on Windows; UCRT runtime; 32-bit builds dropped; `Rstart` gains `RstartVersion` and `R_DefParamsEx`; graphics engine 15 (groups, stroke and fill paths) | Output encoding handled at runtime on this branch. The controller still calls `R_DefParams`, which leaves the version at 0 and keeps the binary loadable under older R. Win32 configurations no longer have an R to link against |
| 4.3.0 | `Rcomplex` becomes a union with a `_Complex` member | Not compilable by MSVC; `R_LEGACY_RCOMPLEX` selects the old layout. See `BUILDING.md` |
| 4.4.0 | Graphics engine 16 (glyph rendering) | Same as 4.1: opt in by version, or leave it |
| 4.5.0 | `Rstart` boolean fields become `int`; R starts hiding entry points that are not part of the API | The tree compiles against 4.5.2 headers so the struct matches. The controller is an embedding front end and uses the documented front-end entry points. `controlr_common.h` also declares four internal symbols by hand (`R_Visible`, `Rf_PrintWarnings`, `R_RestoreGlobalEnvFromFile`, `R_SaveGlobalEnvToFile`); none is referenced, so the declarations can go |

## Encoding: correct Unicode on every R

The 4.2.0 switch to UTF-8 is the change most likely to produce wrong
characters rather than a crash, and it cuts both ways: code written for
code-page R mangles UTF-8, and code written for UTF-8 R mangles code-page
strings. The controller now handles all four paths on the `console-upgrade`
branch (see the encoding note at the top of
`ControlR/src/rinterface_common.cc`):

1. **R strings out to Excel and the console** go through
   `Rf_translateCharUTF8`, which honours the encoding mark each string
   carries, instead of a validity heuristic and a code-page conversion.
2. **Strings in from Excel and the console** are marked UTF-8
   (`Rf_mkCharLenCE` with `CE_UTF8`). The add-in converts Excel's UTF-16
   strings and COM names to UTF-8, so the bytes are UTF-8 on every R version.
3. **Console output and input** pass through untouched when R's native
   encoding is UTF-8, which needs both R 4.2.0 or later and a process with
   the UTF-8 code page. `ControlR.exe` now carries the same manifest setting
   R's own executables use to get that code page (see `BUILDING.md`); the
   controller checks `GetACP()` at startup and otherwise converts to and
   from the code page. Console messages are also built from the length R
   supplies rather than a terminating NUL, which the stricter UTF-8 checks
   in google-protobuf 4 turned from a latent bug into dropped messages.
4. **Paths** are the remaining gap. The add-in lists the functions directory
   with the ANSI file APIs, so file names reach the controller in the Windows
   code page, and `source()` is given them as native strings. That is right
   before 4.2.0 and wrong after it for names outside ASCII. The fix belongs
   in the add-in: use the wide file APIs and send UTF-8, then mark the path
   UTF-8 in the controller like every other string.

`ValidUTF8` and `WindowsCPToUTF8` now serve only the pre-4.2 console
output path.

## Graphics devices

`console_graphics_device.cc` and `spreadsheet_graphics_device.cc` fill in a
`DevDesc` and hand it to `GEaddDevice2`. Since 4.1.0 R reads
`deviceVersion` to decide which callbacks exist. Set it explicitly to
`R_GE_definitions` (13) to declare the pre-4.1 surface the devices actually
implement, and confirm the descriptor is zero-initialised so the unset
callbacks are null. Implementing the 4.1 and later callbacks (patterns, clip
paths, masks, groups, glyphs) is optional; without them R falls back to
plain fills, which is what happens today.

## Toolchain and libraries

* **Protobuf.** Done. Keep `protoc` and `libprotobuf` at the same version:
  the generated C++ refuses to compile against any other. Regenerate with
  the `protoc` from the vcpkg install
  (`ControlR/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe`).
  `PB/build.sh` still points at a protoc 3.5 checkout and needs updating.
  The JavaScript generator was split out of the main project after 3.21, so
  the console's `google-protobuf` stays on 3.21 until the console moves to
  the `protobuf-javascript` package.
* **Stale project settings.** `BERT.vcxproj` and the Win32 configurations
  of `ControlR.vcxproj` still list `../protobuf-3.5.0` and `E:\BERT2` paths
  that vcpkg has made irrelevant. Remove them. Decide whether to keep the
  Win32 configurations at all: 32-bit Excel users would need R 4.1 or
  older, and the controllers would have to be built against that R.
* **Console.** Every dependency is several major versions behind. Electron
  is the hard part: current Electron isolates the renderer from Node by
  default (`contextIsolation`, no `remote` module), and the console is
  written against direct Node access in the renderer. The order that keeps
  each step buildable is TypeScript first (compile-only changes), then
  xterm, monaco and rxjs one at a time (each has API changes), and Electron
  last, since it forces the main/renderer restructuring. Take a fresh look
  at whether the console should instead be rebuilt on a current stack once
  the R side is settled; the console's job is well defined and its
  protobuf interface is stable.
* **Julia.** The 0.6 and 0.7 embedding API does not exist in Julia 1.x, so
  supporting current Julia is a rewrite of the controller, not an upgrade.
  Decide whether to drop it. Either way it is separate from the R work.
* **Installer.** Done on the `console-upgrade` branch: the NSIS script no
  longer bundles R, is 64-bit only, finds the installed R in the registry
  and writes it into `bert-languages.json`, and leaves the user's settings
  alone when installing over an existing BERT 2. `Install\build-release.ps1`
  produces the installer and a zip. The installer is not code-signed, so
  SmartScreen warns on first run.
* **CI.** A GitHub Actions workflow on a Windows runner can build `ControlR`
  and the add-in with MSBuild and cache the vcpkg install. The hosted image
  may not carry the v145 toolset; `/p:PlatformToolset` can override it for
  the CI build.
* **Tests.** The controller can be driven without Excel: start
  `ControlR.exe -p <pipe> -r <R home>`, connect to the pipe, send an exec
  message from the protobuf schema and check the reply. A small harness that
  does this, in Node or Python, gives a smoke test for every R version on
  the machine and a place to put encoding round-trip tests. It would run in
  CI.

## An install without the ribbon

Built on the `xll-only` branch, not merged. The ribbon is a COM add-in: it
has to be registered, Excel can disable it on its own, and it is the only
part of the install that reaches outside BERT's own directory. Dropping it
was not possible before, for two reasons that were not obvious:

* The ribbon **loads the xll**. `CConnect::OnConnection` calls
  `Application.RegisterXLL`, and nothing else does, so with the ribbon
  disabled BERT did not load at all -- no functions, no console. An install
  without it has to register `BERT64.xll` in Excel's own add-in list.
* The ribbon was also the only source of Excel's `Application` pointer,
  which drawing into a sheet and the `EXCEL` object in R both need. Without
  it, graphics silently drew nothing.

The branch fixes both: the installer registers the xll directly when the
ribbon component is cleared, and the add-in asks Excel for the Application
pointer itself at load. Verified with the ribbon disabled -- functions,
console, `EXCEL` object and graphics all work, and a plot lands in the
sheet exactly as it does with the ribbon installed. See `docs/XLL-ONLY.md`,
which also records why the pointer has to be taken at load and not lazily:
asking for the object model while a cell is calculating hangs Excel.

## Suggested order

1. Land `r4-support`.
2. Encoding, items 1 to 3 above. Small, high value, and the harness in the
   next step is the way to test it.
3. The smoke-test harness, then CI running it against the R versions the
   runner can install.
4. The graphics device `deviceVersion` change.
5. Remove the stale project settings and Win32 configurations; decide on
   Julia.
6. The console.
7. The installer.
