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

#pragma once

/**
 * class implements simple registry routine wrappers 
 * for DWORD, string (should add binary for dates?)
 */
class CRegistryUtils
{
public:

	/** set a dword val in the registry */
	static void SetRegDWORD( HKEY hkeyProgramRoot, DWORD dw, const char *szKey, const char *szValName = 0)
	{
		// root: HKLM or HKCU?
		HKEY hKey = 0;
		if( ERROR_SUCCESS == RegCreateKeyExA( hkeyProgramRoot, szKey, 0, 0, 
											 REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &hKey, 0 ))
		{
			RegSetValueExA( hKey, szValName, 0, REG_DWORD, (LPBYTE)&dw, sizeof(DWORD));
			::RegCloseKey( hKey );
		}
	}

	/** retrieve a dword from the registry */
	static BOOL GetRegDWORD( HKEY hkeyProgramRoot, DWORD *pst, const char *szKey, const char *szValName = 0 )
	{
		// root: HKLM or HKCU?
		BOOL bRslt = 0;
		HKEY hKey = 0;
		DWORD dwType = REG_DWORD;
		DWORD dwLen = sizeof(DWORD);
	
		if( ERROR_SUCCESS == RegCreateKeyExA( hkeyProgramRoot, szKey, 0, 0, REG_OPTION_NON_VOLATILE, 
											 KEY_ALL_ACCESS, 0, &hKey, 0 ))
		{
			memset( pst, 0, dwLen );
			if( ERROR_SUCCESS != RegQueryValueExA( hKey, szValName, 0, &dwType, (LPBYTE)pst, &dwLen ))
			{
				memset( pst, 0, dwLen );
			}
			else bRslt = TRUE;
			::RegCloseKey( hKey );
		}
		return bRslt;
	}


	/**
	* retrieve a string from the registry.
	* @returns TRUE on success, FALSE on error.
	*/
	static BOOL GetRegString(HKEY hkeyProgramRoot, char *szTarget, int iBufferLen, const char *szKey, const char *szValName = 0)
	{
		// root: HKLM or HKCU?
		BOOL bRslt = 0;
		HKEY hKey = 0;
		DWORD dwType = REG_SZ;
		DWORD dwLen = iBufferLen - 1;

		if (ERROR_SUCCESS == RegCreateKeyExA(hkeyProgramRoot, szKey, 0, 0, REG_OPTION_NON_VOLATILE,
			KEY_READ, 0, &hKey, 0))
		{
			memset(szTarget, 0, iBufferLen);
			LSTATUS stat = RegQueryValueExA(hKey, szValName, 0, &dwType, (LPBYTE)szTarget, &dwLen);
			if (ERROR_SUCCESS != stat)
			{
				memset(szTarget, 0, iBufferLen);
				//char szMessage[64];
				//sprintf(szMessage, "ERR: %d", stat);
				//::MessageBox(0, szMessage, "ERR", MB_OK);
			}
			else bRslt = TRUE;
			::RegCloseKey(hKey);
		}

#ifndef _WIN64

		// not found? try the 64-bit registry [FIXME: only if we are 32-bit ]
		// NOTE: this is really only necessary for system fields (not ours).

		if (!bRslt)
		{
			if (ERROR_SUCCESS == RegCreateKeyExA(hkeyProgramRoot, szKey, 0, 0, REG_OPTION_NON_VOLATILE,
				KEY_READ | KEY_WOW64_64KEY, 0, &hKey, 0))
			{
				memset(szTarget, 0, iBufferLen);
				LSTATUS stat = RegQueryValueExA(hKey, szValName, 0, &dwType, (LPBYTE)szTarget, &dwLen);
				if (ERROR_SUCCESS != stat)
				{
					memset(szTarget, 0, iBufferLen);
				}
				else bRslt = TRUE;
				::RegCloseKey(hKey);
			}

		}

#endif

		return bRslt;
	}
	
	/**
	* retrieve a string from the registry.
	* @returns TRUE on success, FALSE on error.
	*/
	static BOOL GetRegExpandString(HKEY hkeyProgramRoot, char *szTarget, int iBufferLen, const char *szKey, const char *szValName = 0, bool expand = true)
	{
		// root: HKLM or HKCU?
		BOOL bRslt = 0;
		HKEY hKey = 0;
		DWORD dwType = REG_EXPAND_SZ;

		DWORD dwLen = MAX_PATH- 1;
		char buffer[MAX_PATH];

		if (ERROR_SUCCESS == RegCreateKeyExA(hkeyProgramRoot, szKey, 0, 0, REG_OPTION_NON_VOLATILE,
			KEY_READ, 0, &hKey, 0))
		{
			memset(szTarget, 0, iBufferLen);
			LSTATUS stat = RegQueryValueExA(hKey, szValName, 0, &dwType, (LPBYTE)buffer, &dwLen);
			if (ERROR_SUCCESS != stat)
			{
				memset(szTarget, 0, iBufferLen);
			}
			else
			{
				if ( expand )	
					ExpandEnvironmentStringsA(buffer, szTarget, iBufferLen);
				else strcpy_s(szTarget, iBufferLen, buffer);
				bRslt = TRUE;
			}
			::RegCloseKey(hKey);
		}

#ifndef _WIN64

		// not found? try the 64-bit registry [FIXME: only if we are 32-bit ]
		// NOTE: this is really only necessary for system fields (not ours).

		if (!bRslt)
		{
			if (ERROR_SUCCESS == RegCreateKeyExA(hkeyProgramRoot, szKey, 0, 0, REG_OPTION_NON_VOLATILE,
				KEY_READ | KEY_WOW64_64KEY, 0, &hKey, 0))
			{
				memset(szTarget, 0, iBufferLen);
				LSTATUS stat = RegQueryValueExA(hKey, szValName, 0, &dwType, (LPBYTE)buffer, &dwLen);
				if (ERROR_SUCCESS != stat)
				{
					memset(szTarget, 0, iBufferLen);
				}
				else
				{
					ExpandEnvironmentStringsA(buffer, szTarget, iBufferLen);
					bRslt = TRUE;
				}
				::RegCloseKey(hKey);
			}

		}

#endif

		return bRslt;
	}

	/** store a string in the registry.  */
	static void SetRegString(HKEY hkeyProgramRoot, const char *szCode, const char *szKey, const char *szValName = 0)
	{
		// root: HKLM or HKCU?
		HKEY hKey = 0;
		if (ERROR_SUCCESS == RegCreateKeyExA(hkeyProgramRoot, szKey, 0, 0, REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS, 0, &hKey, 0))
		{
			RegSetValueExA(hKey, szValName, 0, REG_SZ, (LPBYTE)szCode, (DWORD)(strlen(szCode)));
			::RegCloseKey(hKey);
		}
	}
	
	/** store a string in the registry.  */
	static void SetRegExpandString(HKEY hkeyProgramRoot, const char *szCode, const char *szKey, const char *szValName = 0)
	{
		// root: HKLM or HKCU?
		HKEY hKey = 0;
		if (ERROR_SUCCESS == RegCreateKeyExA(hkeyProgramRoot, szKey, 0, 0, REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS, 0, &hKey, 0))
		{
			RegSetValueExA(hKey, szValName, 0, REG_EXPAND_SZ, (LPBYTE)szCode, (DWORD)(strlen(szCode)));
			::RegCloseKey(hKey);
		}
	}

};


