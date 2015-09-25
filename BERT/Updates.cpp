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
#include "updates.h"
#include "DebugOut.h"

#include <winhttp.h>


double parseVersion(const char *v) {
	double version = -1;
	if (v[0] == 'v') version = atof(v + 1);
	else version = atof(v);
	return version;
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

	int result = UC_UPDATE_CHECK_FAILED;

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

	if (hRequest)
		bResults = WinHttpSendRequest(hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

	if (bResults)
		bResults = WinHttpReceiveResponse(hRequest, NULL);

	// FIXME: use etag header 

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

	if (str.length() > 0) {

		static std::regex rex("\"tag_name\"\\s*:\\s*\"(.*?)\"");
		std::smatch mat;
		if (std::regex_search(str, mat, rex) && mat.size() > 1)
		{
			tag.assign(mat[1].first, mat[1].second);
			double v = parseVersion(tag.c_str());
			double bv = _wtof(BERT_VERSION);

			DebugOut("Version string: %s, reads as %f, bert is %f\n", tag.c_str(), v, bv);

			if (v > bv) result = UC_NEW_VERSION_AVAILABLE;
			else if (v < bv) result = UC_YOURS_IS_NEWER;
			else result = UC_UP_TO_DATE;

		}

	}

	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);

	return result;

}
