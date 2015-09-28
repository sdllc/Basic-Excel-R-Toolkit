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

#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <iostream>
#include <time.h>

#include "util.h"
#include "DebugOut.h"
#include "Objbase.h"

extern HRESULT SafeCall(SAFECALL_CMD cmd, std::vector< std::string > *vec, int *presult);

typedef std::vector<std::string> STRVECTOR;

HANDLE hThread = 0;
STRVECTOR files;

DWORD threadID = 0;
HANDLE hSignal = 0;

DWORD WINAPI execFunctionThread(void *parameter) {

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	int pr;
	std::string str((const char*)parameter);
	STRVECTOR svec;
	svec.push_back(str);
	SafeCall(SCC_WATCH_NOTIFY, &svec, &pr);
	::CoUninitialize();
	return 0;

}

DWORD WINAPI startWatchThread(void *parameter) {

	STRVECTOR dirs;
	HANDLE *handles = 0;
	char dir[256];
	char drive[32];
	bool errflag = false;

	DebugOut("Watch thread start\n");

	/*
	const char *eventid = (const char*)(parameter);
	HANDLE hevent = ::OpenEventA(SYNCHRONIZE, 0, eventid);

	if (!hevent) {
		errflag = true;

		DWORD dw = GetLastError();

		LPVOID lpMsgBuf;

		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&lpMsgBuf,
			0, NULL);

		//std::cout << "ERR opening event: " << (char*)lpMsgBuf << std::endl;
		DebugOut("ERR opening event: %s", lpMsgBuf);

		free(lpMsgBuf);

		// LocalFree(lpMsgBuf);
	}
	*/

	std::map < std::string, FILETIME > fmap;

	// cache file times.  also consolidate directories

	for (STRVECTOR::iterator iter = files.begin(); iter != files.end(); iter++) {

		HANDLE hfile = ::CreateFileA(iter->c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

		if (!hfile || hfile == INVALID_HANDLE_VALUE) {
			errflag = true;
			//std::cout << "ERR opening file " << iter->c_str() << std::endl;
			DebugOut("ERR opening file %s\n", iter->c_str());
			break;
		}

		FILETIME ft;
		::GetFileTime(hfile, 0, 0, &ft);
		fmap.insert(std::pair< std::string, FILETIME >(*iter, ft));

		::CloseHandle(hfile);

		_splitpath_s(iter->c_str(), drive, 32, dir, 256, 0, 0, 0, 0);

		std::string dirpath = drive;
		dirpath.append(dir);

		if (std::find(dirs.begin(), dirs.end(), dirpath) == dirs.end()) {
			dirs.push_back(dirpath);
		}

	}

	// create watch handles


	if (dirs.size() == 0) return -1;
	if (dirs.size() >= MAXIMUM_WAIT_OBJECTS - 1) {

//		std::cout << "Too many watch handles" << std::endl;
		DebugOut("Too many watch handles\n");
		errflag = true;

	}

	handles = new HANDLE[dirs.size() + 1];
	handles[0] = hSignal; //  hevent;

	int index = 1;
	DWORD filter = FILE_NOTIFY_CHANGE_LAST_WRITE;

	for (STRVECTOR::iterator iter = dirs.begin(); !errflag && iter != dirs.end(); iter++, index++) {

		handles[index] = FindFirstChangeNotificationA(iter->c_str(), 0, filter);
		if (!handles[index] || handles[index] == INVALID_HANDLE_VALUE)
		{
			DebugOut("ERROR: FindFirstChangeNotification function failed.\n");
			errflag = true;
		}

	}

	DWORD stat, dwlen;
	char buffer[1024];

	while (!errflag) {

		stat = WaitForMultipleObjects(index, handles, 0, INFINITE);
		if (stat == WAIT_OBJECT_0) {

			DebugOut(" * user exit\n");
			errflag = true;

		}
		else if (stat > WAIT_OBJECT_0 && stat < WAIT_OBJECT_0 + index) {

			int key = stat - WAIT_OBJECT_0;
			// std::cout << "signal " << key << " " << dirs[key-1].c_str() << std::endl;

			for (STRVECTOR::iterator iter = files.begin(); iter != files.end(); iter++) {

				// FIXME: check if path =~ file dir

				// we will often see an error here.  that's because (notwithstanding the flags),
				// we get multiple updates on the file; and in the first one, there may be a 
				// write lock.  in that event, wait for the next one.  well-behaved editors won't
				// hold that write lock.

				HANDLE hfile = ::CreateFileA(iter->c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
				if (hfile && hfile != INVALID_HANDLE_VALUE) {

					FILETIME ft;
					::GetFileTime(hfile, 0, 0, &ft);

					std::map< std::string, FILETIME > ::iterator i2 = fmap.find(*iter);
					if (i2 != fmap.end()) {
						if (ft.dwHighDateTime != i2->second.dwHighDateTime
							|| ft.dwLowDateTime != i2->second.dwLowDateTime)
						{

							DebugOut("Change in file: %s\n", iter->c_str());
							// WatchFileCallback(iter->c_str());

							DWORD dwID;
							CreateThread(0, 0, execFunctionThread, (void*)(iter->c_str()), 0, &dwID);

							i2->second.dwHighDateTime = ft.dwHighDateTime;
							i2->second.dwLowDateTime = ft.dwLowDateTime;

						}
					}

					::CloseHandle(hfile);
				}
			}

			if (!FindNextChangeNotification(handles[key]))
			{
				DebugOut("ERROR: FindNextChangeNotification function failed.\n");
				errflag = true;
			}

		}
		else {
			DebugOut("unexpected status: 0x%x\n", stat);
			errflag = true;
		}

	}

	// clean up handles

	for (int i = 1; i < index; i++) {

		if (handles[i] && handles[i] != INVALID_HANDLE_VALUE) {
			FindCloseChangeNotification(handles[i]);
		}

	}

	delete[] handles;

	DebugOut("Watch thread exit\n");
	

	return 0;
}

void stopFileWatcher() {

	if (threadID) {

		::SetEvent(hSignal);
		::WaitForSingleObject(hThread, INFINITE);
		::CloseHandle(hSignal);
		::CloseHandle(hThread);

	}

	threadID = 0;
	hSignal = 0;
	hThread = 0;

}

int watchFiles(STRVECTOR &list) {

	char szEvent[MAX_PATH];

	// if we are already watching files, stop
	stopFileWatcher();

	// rebuild the watch list. also verify we can 
	// read the file; if not, return an error

	files.clear();
	for (STRVECTOR::iterator iter = list.begin(); iter != list.end(); iter++) {
		HANDLE hfile = ::CreateFileA(iter->c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		if (!hfile || hfile == INVALID_HANDLE_VALUE) return -1;
		::CloseHandle(hfile);
		files.push_back(iter->c_str());
	}

	if (files.size() == 0) return 0;

	DWORD dwpid = GetCurrentProcessId();
	DWORD dwtime = time(0);

	sprintf_s(szEvent, "BERT-Event-%u-%u", dwpid, dwtime);

	hSignal = ::CreateEventA(0, 0, 0, szEvent);
	hThread = ::CreateThread(0, 0, startWatchThread, szEvent, 0, &threadID);

	return 0;
}

