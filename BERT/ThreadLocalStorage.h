/*
 * Basic Excel R Toolkit (BERT)
 * Copyright (C) 2014-2015 Structured Data, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */


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
