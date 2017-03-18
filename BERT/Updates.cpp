/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014-2017 Structured Data, LLC
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
#include "updates.h"
#include "RegistryConstants.h"
#include "RegistryUtils.h"
#include "DebugOut.h"
#include "util.h"

#include <winhttp.h>

/**
 * heading towards support for semver (major.minor.patch)
 */
double parseVersion(const char *v) {
	
	double version = 0;

	std::string vstr;
	if (v[0] == 'v') vstr = (v+1);
	else vstr = v;

	std::vector< std::string > elements;
	Util::split(vstr, '.', 1, elements);

	if (elements.size() > 0) version += atof(elements[0].c_str()) * 1000 * 1000;
	if (elements.size() > 1) version += atof(elements[1].c_str()) * 1000;
	if (elements.size() > 2) version += atof(elements[2].c_str());

	return version;
}

double parseVersionW(const WCHAR *v) {
	char buffer[256];
	int len = wcslen(v);
	if (len > 255) len = 255;
	for (int i = 0; i < len; i++) buffer[i] = v[i] & 0xff;
	return parseVersion(buffer);
}

/**
 *
 */
int checkForUpdates( std::string &tag )
{
	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;
	LPSTR pszOutBuffer;
	BOOL  bResults = FALSE;
	HINTERNET  hSession = NULL,
		hConnect = NULL,
		hRequest = NULL;
	int datalen = 0;
	std::string str;

	char szLastTag[MAX_PATH];
	char szDateBuffer[MAX_PATH];
	WCHAR wszDateBuffer[MAX_PATH] = L"If-Modified-Since: ";
	DWORD dwResponseCode;

	int result = UC_UPDATE_CHECK_FAILED;

	// read last modified, last tag from registry.  we generally deal in one-byte 
	// registry values.  with winhttp, headers are two-byte strings, but the data
	// comes back as one-byte (could be utf-8, though).

	// we'll just treat it as one-byte.  for the date, if we send no accept headers,
	// we'll get default language english and the date will be narrow.  we also
	// assume a max length on the string, which should be reasonable (that's the 
	// kind of statement that gets you into trouble).

	if (CRegistryUtils::GetRegString(HKEY_CURRENT_USER, szDateBuffer, MAX_PATH, REGISTRY_KEY, REGISTRY_VALUE_UPDATE_LAST_MODIFIED)) {
		
		int len = strlen(szDateBuffer);
		int idx = wcslen(wszDateBuffer);

		for (int i = 0; i < len; i++, idx++) wszDateBuffer[idx] = szDateBuffer[i];
		wszDateBuffer[idx] = 0;

	}
	else wszDateBuffer[0] = 0;

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, szLastTag, MAX_PATH, REGISTRY_KEY, REGISTRY_VALUE_UPDATE_LAST_TAG)) {
		szLastTag[0] = 0;
	}

	hSession = WinHttpOpen( USER_AGENT_STRING,
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);

	if (hSession)
		hConnect = WinHttpConnect(hSession, GITHUB_HOST,
			INTERNET_DEFAULT_HTTPS_PORT, 0);

	if (hConnect)
		hRequest = WinHttpOpenRequest(hConnect, L"GET", RELEASE_URL,
			NULL, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE);

	// if we have a registry value for the last-modified date, 
	// pass it as a cache control header.  good behavior, and 
	// prevents github from throttling you.

	if (hRequest && wszDateBuffer[0])
		WinHttpAddRequestHeaders(hRequest, wszDateBuffer, wcslen(wszDateBuffer), WINHTTP_ADDREQ_FLAG_ADD);
	
	if (hRequest)
		bResults = WinHttpSendRequest(hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

	if (bResults)
		bResults = WinHttpReceiveResponse(hRequest, NULL);

	if (bResults) {
		dwSize = MAX_PATH;
		bResults = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LAST_MODIFIED, WINHTTP_HEADER_NAME_BY_INDEX, wszDateBuffer, &dwSize, WINHTTP_NO_HEADER_INDEX);
	}

	if (bResults) {
		dwSize = sizeof(DWORD);
		bResults = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwResponseCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
	}

	if (bResults)
	{
		datalen = 0;
		do
		{
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
				// printf("Error %u in WinHttpQueryDataAvailable.\n", GetLastError());
			}

			pszOutBuffer = new char[dwSize + 1];
			if (!pszOutBuffer)
			{
				//printf("Out of memory\n");
				dwSize = 0;
			}
			else if( dwSize )
			{
				ZeroMemory(pszOutBuffer, dwSize + 1);
				if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
					// printf("Error %u in WinHttpReadData.\n", GetLastError());
				}
				// else printf("%s", pszOutBuffer);
				str.append(pszOutBuffer);
				delete[] pszOutBuffer;
			}
		} 
		while (dwSize > 0);
	}

	// if response code is 304, use the cached value.  otherwise
	// use the returned data, and cache tag and header.

	if( dwResponseCode == 200 && str.length() > 0) {
		static std::regex rex("\"tag_name\"\\s*:\\s*\"(.*?)\"");
		std::smatch mat;
		if (std::regex_search(str, mat, rex) && mat.size() > 1)
		{
			tag.assign(mat[1].first, mat[1].second);
		}
	}
	else if (dwResponseCode == 304) {
		tag = szLastTag;
	}

	if( tag.length() > 0 )
	{
		double remoteVersion = parseVersion(tag.c_str());
		double localVersion = parseVersionW(BERT_VERSION);

		DebugOut("Version string: %s, reads as %f, bert is %f\n", tag.c_str(), remoteVersion, localVersion);

		if (remoteVersion > localVersion) result = UC_NEW_VERSION_AVAILABLE;
		else if (remoteVersion < localVersion) result = UC_YOURS_IS_NEWER;
		else result = UC_UP_TO_DATE;

		// save last-modified, tag.  see above (top) re: narrow registry strings.

		int len = wcslen(wszDateBuffer);
		for (int i = 0; i < len; i++) { szDateBuffer[i] = wszDateBuffer[i] & 0xff; }

		CRegistryUtils::SetRegString(HKEY_CURRENT_USER, szDateBuffer, REGISTRY_KEY, REGISTRY_VALUE_UPDATE_LAST_MODIFIED);
		CRegistryUtils::SetRegString(HKEY_CURRENT_USER, tag.c_str(), REGISTRY_KEY, REGISTRY_VALUE_UPDATE_LAST_TAG);

	}

	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);

	return result;

}
