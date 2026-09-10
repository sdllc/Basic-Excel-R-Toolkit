# Builds the pieces of a release and packages them: the native projects,
# the BERTModule R package, the console, the installer and a zip.
#
#   .\build-release.ps1 -Version 2.4.3-r4
#
# Prerequisites: Visual Studio 2026 (msbuild on the path or found below),
# R 4.2 or later with Rtools for the module, node for the console, and NSIS
# (makensis) for the installer. Each stage can be skipped.
#
# Output, in ..\Build: the native binaries, module\, Console\win-unpacked\,
# and in this directory BERT-Installer-<version>-x64.exe and
# BERT-<version>-x64.zip.

Param(
  [string]$Version = "",
  [string]$RHome = "",
  [string]$MakeNsis = "makensis",
  # BERTModule is compiled against R's graphics engine, whose version changes
  # between R series, so one module per series is built and shipped. Give the
  # R installations to build against; the default is the R used for the rest
  # of the build. Each needs the matching Rtools on the PATH.
  [string[]]$ModuleRHomes,

  # Take the modules from the "graphics modules" workflow instead of building
  # them here. That workflow builds one per series on runners, which saves
  # installing an R and its matching Rtools for every series on this machine.
  # Needs the gh cli, signed in.
  [switch]$FetchModules,

  [switch]$SkipNative,
  [switch]$SkipModule,
  [switch]$SkipConsole,
  [switch]$SkipInstaller,
  [switch]$SkipZip
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here "..")
$build = Join-Path $root "Build"

function Step($text) { Write-Host "`n== $text" -ForegroundColor Cyan }
function Fail($text) { Write-Host $text -ForegroundColor Red; exit 1 }

# ---- version: from the add-in's header unless given ------------------------
if (-not $Version) {
  $header = Get-Content (Join-Path $root "BERT\BERT\include\bert_version.h") -Raw
  $number = [regex]::Match($header, 'BERT_VERSION_NUMBER\s+L"([^"]+)"').Groups[1].Value
  $tag = [regex]::Match($header, 'BERT_VERSION_TAG\s+L"([^"]*)"').Groups[1].Value
  if (-not $number) { Fail "could not read the version from bert_version.h" }
  $Version = "$number$tag"
}
Write-Host "release version: $Version"

# ---- R: the registered current version unless given ------------------------
if (-not $RHome) {
  foreach ($key in "HKCU:\SOFTWARE\R-core\R64", "HKLM:\SOFTWARE\R-core\R64", "HKCU:\SOFTWARE\R-core\R", "HKLM:\SOFTWARE\R-core\R") {
    if (Test-Path $key) { $RHome = (Get-ItemProperty $key).InstallPath; if ($RHome) { break } }
  }
}
if (-not $RHome -or -not (Test-Path "$RHome\bin\x64\R.exe")) { Fail "no 64-bit R found; pass -RHome" }
Write-Host "R: $RHome"

# ---- native ----------------------------------------------------------------
if (-not $SkipNative) {
  Step "native projects (Release x64)"
  $msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
  if ($msbuild) { $msbuild = $msbuild.Source }
  else {
    $candidates = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\MSBuild\Current\Bin\MSBuild.exe" -ErrorAction SilentlyContinue
    if (-not $candidates) { Fail "msbuild not found" }
    $msbuild = $candidates[0].FullName
  }
  & $msbuild (Join-Path $root "BERT\BERT.sln") -p:Configuration=Release -p:Platform=x64 -m -nologo -v:minimal
  if ($LASTEXITCODE) { Fail "native build failed" }
}

# ---- R module --------------------------------------------------------------
if (-not $SkipModule) {

  $root_lib = Join-Path $build "module"
  if (Test-Path $root_lib) { Remove-Item $root_lib -Recurse -Force }
  New-Item -ItemType Directory $root_lib | Out-Null

  $homes = if ($FetchModules) { @() } elseif ($ModuleRHomes) { $ModuleRHomes } else { @($RHome) }

  if ($FetchModules) {

    Step "modules from the last successful 'graphics modules' run"

    # name the repository: this clone also has an upstream remote, and gh
    # otherwise resolves there, where the workflow does not exist
    $repo = (& git -C $root remote get-url origin) -replace '.*github\.com[/:]', '' -replace '\.git$', ''
    Write-Host "  repository: $repo"

    $run_id = & gh run list --repo $repo --workflow "graphics modules" --status success --limit 1 --json databaseId -q ".[0].databaseId"
    if ($LASTEXITCODE -or -not $run_id) { Fail "could not find a successful module build; run the workflow first" }

    $staging = Join-Path $build "module-artifacts"
    if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
    New-Item -ItemType Directory $staging | Out-Null

    & gh run download $run_id --repo $repo --dir $staging
    if ($LASTEXITCODE) { Fail "could not download the module artifacts" }

    foreach ($dir in Get-ChildItem $staging -Directory) {
      # artifacts are named BERTModule-<series>
      $series = $dir.Name -replace '^BERTModule-', ''
      if (-not (Test-Path (Join-Path $dir.FullName "BERTModule/DESCRIPTION"))) {
        Fail "artifact $($dir.Name) does not contain a BERTModule"
      }
      $built = (Select-String -Path (Join-Path $dir.FullName "BERTModule/DESCRIPTION") -Pattern "^Built:").Line
      if ($built -notmatch [regex]::Escape("R $series")) {
        Fail "artifact $($dir.Name) says '$built', which is not R $series"
      }
      Copy-Item $dir.FullName (Join-Path $root_lib $series) -Recurse
      Write-Host "  $series <- run $run_id   $built"
    }

    if (-not (Get-ChildItem $root_lib -Directory)) { Fail "no modules were downloaded" }
    Remove-Item $staging -Recurse -Force
  }

  foreach ($r_home in $homes) {

    if (-not (Test-Path "$r_home\bin\x64\R.exe")) { Fail "no 64-bit R at $r_home" }

    $series = & "$r_home\bin\x64\Rscript.exe" --vanilla -e "cat(paste0(R.version`$major, '.', strsplit(R.version`$minor, '.', fixed=TRUE)[[1]][1]))"
    Step "BERTModule for R $series (R CMD INSTALL with $r_home)"

    # object files must not be carried between R versions: R CMD INSTALL will
    # happily relink a module from objects compiled against another R's
    # headers, and the result fails R_GE_checkVersionOrDie at run time with
    # "Graphics API version mismatch" -- which looks like a code fault and is
    # not one.

    Get-ChildItem (Join-Path $root "Module\src") -Include *.o, *.dll -Recurse -ErrorAction SilentlyContinue |
      Remove-Item -Force -ErrorAction SilentlyContinue

    $lib = Join-Path $root_lib $series
    New-Item -ItemType Directory $lib | Out-Null
    & "$r_home\bin\x64\R.exe" CMD INSTALL --library="$lib" --no-multiarch (Join-Path $root "Module")
    if ($LASTEXITCODE) { Fail "module build failed for R $series" }

    $built = (Select-String -Path (Join-Path $lib "BERTModule\DESCRIPTION") -Pattern "^Built:").Line
    Write-Host "  $series -> $built"
  }
}

# ---- console ---------------------------------------------------------------
if (-not $SkipConsole) {
  Step "console (npm run repackage)"
  Push-Location (Join-Path $root "Console")
  try {
    if (-not (Test-Path "node_modules")) { npm install --no-audit --no-fund; if ($LASTEXITCODE) { Fail "npm install failed" } }
    npm run repackage
    if ($LASTEXITCODE) { Fail "console build failed" }
  } finally { Pop-Location }
}

# ---- what a release contains -----------------------------------------------
$files = "BERT64.xll", "BERTRibbon2x64.dll", "ControlR.exe",
         "bert-config-template.json", "user-stylesheet-template.less", "Welcome.md",
         "CHANGELOG.md", "bert2.ico"

# shipped from this directory rather than from Build. the IntelliSense add-in
# is a third-party binary, pinned by version and checked against its hash
# rather than committed to the repository.
$installer_files = "BERT-IntelliSense.xlam", "ExcelDna.IntelliSense64.xll", "ExcelDna.IntelliSense-License.txt",
                   "BERT-IntelliSense.intellisense.xml"

# the icon ships in the payload but is checked in here, next to the installer
# script that also uses it; copy rather than keep a second copy in Build
Copy-Item (Join-Path $here "bert2.ico") (Join-Path $build "bert2.ico") -Force
$intellisense_version = "v1.9.0"
$intellisense_sha256 = "311E8A0520330EC0C5868EFA2D97E37DF3401AEDED3481014336DD8308A79A8A"

# the binaries must not need the Visual C++ redistributable: everything is
# linked against the static runtime, protobuf included (x64-windows-static)
foreach ($f in "BERT64.xll", "BERTRibbon2x64.dll", "ControlR.exe") {
  $bytes = [IO.File]::ReadAllBytes((Join-Path $build $f))
  $text = [Text.Encoding]::ASCII.GetString($bytes)
  foreach ($dll in "VCRUNTIME140", "MSVCP140", "libprotobuf.dll", "abseil_dll.dll") {
    if ($text.IndexOf($dll, [StringComparison]::OrdinalIgnoreCase) -ge 0) { Fail "$f imports $dll; build with the static vcpkg triplet" }
  }
}
$dirs = "Console", "module", "startup"
foreach ($f in $files) { if (-not (Test-Path (Join-Path $build $f))) { Fail "missing $f in Build" } }
if (-not (Test-Path (Join-Path $root "README.md"))) { Fail "missing README.md in the repository root" }
foreach ($d in $dirs) { if (-not (Test-Path (Join-Path $build $d))) { Fail "missing $d\ in Build" } }

# ---- the optional function help add-in -------------------------------------
$isxll = Join-Path $here "ExcelDna.IntelliSense64.xll"
if (-not (Test-Path $isxll)) {
  Step "downloading Excel-DNA IntelliSense $intellisense_version"
  $url = "https://github.com/Excel-DNA/IntelliSense/releases/download/$intellisense_version/ExcelDna.IntelliSense64.xll"
  Invoke-WebRequest -Uri $url -OutFile $isxll
}
$hash = (Get-FileHash $isxll -Algorithm SHA256).Hash
if ($hash -ne $intellisense_sha256) {
  Fail "ExcelDna.IntelliSense64.xll does not match the pinned hash`n  expected $intellisense_sha256`n  found    $hash"
}
Write-Host "IntelliSense add-in $intellisense_version verified"

# ---- installer -------------------------------------------------------------
if (-not $SkipInstaller) {
  Step "installer (makensis)"
  if (-not (Get-Command $MakeNsis -ErrorAction SilentlyContinue)) {
    $found = "${env:ProgramFiles(x86)}\NSIS\makensis.exe", "$env:ProgramFiles\NSIS\makensis.exe" | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $found) { Fail "makensis not found; install NSIS or pass -MakeNsis" }
    $MakeNsis = $found
  }
  Push-Location $here
  try {
    & $MakeNsis "/DVERSION=$Version" install-script.nsi
    if ($LASTEXITCODE) { Fail "makensis failed" }
  } finally { Pop-Location }
  Write-Host "installer: $(Join-Path $here "BERT-Installer-$Version-x64.exe")"
}

# ---- zip -------------------------------------------------------------------
if (-not $SkipZip) {
  Step "zip"
  $stage = Join-Path $env:TEMP "bert-release-$Version"
  if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
  New-Item -ItemType Directory $stage | Out-Null
  foreach ($f in $files) { Copy-Item (Join-Path $build $f) $stage }
  foreach ($f in $installer_files) { Copy-Item (Join-Path $here $f) $stage }
  foreach ($d in $dirs) { Copy-Item (Join-Path $build $d) (Join-Path $stage $d) -Recurse }
  # the languages file with no R filled in: the zip relies on BERT.R.home
  (Get-Content (Join-Path $here "bert-languages.template.json") -Raw).Replace('"@R_HOME@"', '""') |
    Set-Content (Join-Path $stage "bert-languages.json") -NoNewline
  Copy-Item (Join-Path $here "INSTALL-FROM-ZIP.md") $stage

  # the project README ships too, from the repository root: the one in Build
  # describes the build directory and is not for shipping
  Copy-Item (Join-Path $root "README.md") $stage
  $zip = Join-Path $here "BERT-$Version-x64.zip"
  if (Test-Path $zip) { Remove-Item $zip -Force }
  Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal
  Remove-Item $stage -Recurse -Force
  Write-Host "zip: $zip"
}

Write-Host "`ndone" -ForegroundColor Green
