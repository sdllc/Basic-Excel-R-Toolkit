#=============================================
#
# Copyright (c) 2015 Structured Data, LLC.
# All rights reserved.
#
#=============================================

#=============================================
# parameters
#=============================================

Param(	[switch]$x86,
	[switch]$x64,
	[switch]$def,
	[switch]$all,
	[String]$R
);

if( $all )
{
	$def = $TRUE;
	$x86 = $TRUE;
	$x64 = $TRUE;
};

#=============================================
# functions
#=============================================

#---------------------------------------------
# exit on error, with message
#---------------------------------------------
Function ExitOnError( $message ) {

	if( $LastExitCode -ne 0 ){
		if( $message -ne $null ){ Write-Host "$message" -foregroundcolor red ; }
		Write-Host "Exit on error: $LastExitCode`r`n" -foregroundcolor red ;
		Exit $LastExitCode;
	}
	
}

#---------------------------------------------
# create the .def file
#---------------------------------------------
Function GenerateDef( $sub, $key ) {

	Write-Host "Generating $key-bit .defs (R.dll)" -foregroundcolor yellow ;

	$symbols = dumpbin /exports $r\bin\$sub\R.dll | Out-String
	ExitOnError;
	
	$symbols = ($symbols -replace "(?s)^.*ordinal.*?\n", "");
	$symbols = ($symbols -replace "(?s)\n\s*Summary.*?$", "");
	$symbols = ($symbols -replace "(?m)^\s+\S+\s+\S+\s+\S+\s+", "");

	echo "LIBRARY R`nEXPORTS`n`n$symbols`n" | Out-File -encoding ASCII R$key.def
	ExitOnError;

	Write-Host "Generating $key-bit .defs (RGraphApp.dll)" -foregroundcolor yellow ;

	$symbols = dumpbin /exports $r\bin\$sub\RGraphApp.dll | Out-String
	ExitOnError;
	
	$symbols = ($symbols -replace "(?s)^.*ordinal.*?\n", "");
	$symbols = ($symbols -replace "(?s)\n\s*Summary.*?$", "");
	$symbols = ($symbols -replace "(?m)^\s+\S+\s+\S+\s+\S+\s+", "");

	$symbols = ($symbols -replace "(?m)^\(.*?\n", "");

	echo "LIBRARY RGraphApp`nEXPORTS`n`n$symbols`n" | Out-File -encoding ASCII RGraphApp$key.def
	ExitOnError;

}

#---------------------------------------------
# create the lib file from the .def
#---------------------------------------------
Function GenerateLib( $machine, $key ){

	Write-Host "Generating $key-bit libs" -foregroundcolor yellow ;
	lib /machine:$machine /def:R$key.def /out:R$key.lib
	lib /machine:$machine /def:RGraphApp$key.def /out:RGraphApp$key.lib
	ExitOnError;

}

#=============================================
# runtime
#=============================================

Write-Host "";
if( $def ){
	if( $x86 ){ GenerateDef "i386" "32"  }
	if( $x64 ){ GenerateDef "x64" "64"  }
	Write-Host "";
}
if( $x86 ){ GenerateLib "X86" "32" }
if( $x64 ){ GenerateLib "X64" "64" }
Write-Host "";
Write-Host "Done" -foregroundcolor green;
Write-Host "";





