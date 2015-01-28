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

#include "stdafx.h"
#include "BERT.h"
#include "XLCALL.h"
#include "ThreadLocalStorage.h"

#include <stdlib.h>

DWORD TlsIndex; // only needs module scope if all TLS access in this module

// in the event of an allocation or TLS retrieval error, we'll need
// an OPER we can return out of functions.  we'll use this one and
// have it be a constant error message (where to initialize - main?)

XLOPER12 g_xlAllocErr12;

TLS_data *get_TLS_data(void)
{
	// Get a pointer to this thread's static memory
	void *pTLS = TlsGetValue(TlsIndex);

	if (!pTLS) // No TLS memory for this thread yet
	{
		if ((pTLS = calloc(1, sizeof(TLS_data))) == NULL)
			// Display some error message (omitted)
			return NULL;
		TlsSetValue(TlsIndex, pTLS); // Associate this this thread
	}
	return (TLS_data *)pTLS;
}

BOOL TLS_Action(DWORD DllMainCallReason)
{
	switch (DllMainCallReason)
	{
	case DLL_PROCESS_ATTACH: // The DLL is being loaded
		if ((TlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
			return FALSE;
		break;

	case DLL_PROCESS_DETACH: // The DLL is being unloaded 
		TlsFree(TlsIndex); // Release the TLS index.
		break;
	}
	return TRUE;
}

XLOPER * get_thread_local_xloper(void)
{
	TLS_data *pTLS = get_TLS_data();
	if (pTLS) return &(pTLS->xloper_shared_ret_val);
	return NULL;
}

XLOPER12 * get_thread_local_xloper12(void)
{
	TLS_data *pTLS = get_TLS_data();
	if (pTLS) return &(pTLS->xloper12_shared_ret_val);
	return NULL;
}

XLMREF12 * get_thread_local_xlmref12(void)
{
	TLS_data *pTLS = get_TLS_data();
	if (pTLS) return &(pTLS->xlmref12_shared_ret_val);
	return NULL;
}

