// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

extern BOOL TLS_Action(DWORD DllMainCallReason);

HMODULE ghModule;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	ghModule = hModule;
	return TLS_Action(ul_reason_for_call);
}

