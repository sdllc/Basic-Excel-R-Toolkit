//=========================================================
//
// Copyright (c) 2014 Structured Data, LLC.
// All rights reserved.
//
//=========================================================

#ifndef __THREAD_LOCAL_STORAGE_H
#define __THREAD_LOCAL_STORAGE_H

//===============================================================
//
// Thread-local storage for working with Excel12 threads.  Using
// TLS replaces the old static method of returning LPXLOPERs
// from functions.
//
// Most of this comes from the helpful MSDN article about XL12:
// http://msdn2.microsoft.com/en-us/library/aa730920.aspx
//
//===============================================================

extern DWORD		TlsIndex; // only needs module scope if all TLS access in this module
extern XLOPER12		g_xlAllocErr12;

struct TLS_data
{
	XLOPER		xloper_shared_ret_val;
	XLOPER12	xloper12_shared_ret_val;
	XLMREF12	xlmref12_shared_ret_val;
};

BOOL TLS_Action(DWORD DllMainCallReason);
TLS_data *get_TLS_data();
XLOPER * get_thread_local_xloper();
XLOPER12 * get_thread_local_xloper12();
XLMREF12 * get_thread_local_xlmref12();

#endif // #ifndef __THREAD_LOCAL_STORAGE_H
