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

#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <time.h>
#include <cctype>

#include "UtilityContainers.h";

#include "util.h"
#include "DebugOut.h"
#include "Objbase.h"

using namespace std;

extern HRESULT SafeCall(SAFECALL_CMD cmd, vector< string > *vec, long arg, int *presult);

typedef vector<string> STRVECTOR;

HANDLE hThread = 0;

locked_deque< string > changes;
unordered_set< string > directories, files;

DWORD threadID = 0;
HANDLE hSignal = 0;

/**
 * overlapped and change buffer for each watched directory.
 *
 * so long as we're just using pointers we can use containers
 * without a copy ctor.
 */
class FileChangeData {

public:
	HANDLE fileHandle;
	OVERLAPPED overlapped;
	byte buffer[1024];
	string path;
	DWORD dwRead;
	int directory_flag;

public:
	FileChangeData() : fileHandle(0), dwRead(0), directory_flag(0) {
		memset(&overlapped, 0, sizeof(OVERLAPPED));
	}
};


/**
* when R combines paths it uses forward slashes instead of windows
* backslashes.  that might lead to multiple inclusions of the same
* path.  we'll try to normalize that. we also ensure there's a
* trailing slash (optionally).
*
* note that windows has PathCanonicalize functions which we can't
* use; PathCanonicalize is broken and the replacements only go back
* to windows 8 (we're targeting windows 7).
*
* we're making no effort to fix UNC paths or normalize directory
* traversal (atm).
*/
const char * normalize_path(const char *path, bool trailing_slash) {

	static char buffer[MAX_PATH + 1];
	int i, len = strnlen_s(path, MAX_PATH);

	for (i = 0; i < len; i++) {
		if (path[i] == '/') buffer[i] = '\\';
		else buffer[i] = path[i];
	}

	if (trailing_slash && i && i < MAX_PATH && buffer[i - 1] != '\\') buffer[i++] = '\\';

	buffer[i] = 0;
	return buffer;
}

DWORD WINAPI execFunctionThread(void *parameter) {

	deque<string> local;
	changes.locked_consume(local);

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	int pr;
	SVECTOR svec(local.begin(), local.end());
	SafeCall(SCC_WATCH_NOTIFY, &svec, 0, &pr);

	::CoUninitialize();
	return 0;

}

/**
 * for lexicographical_compare; use tolower from cctype.  this is byte-based
 * (because string::iterator is byte-based) so what happens with utf8? (...)
 */
bool icasecomp(char c1, char c2)
{
	return std::tolower(c1)<std::tolower(c2);
}


DWORD WINAPI startWatchThread(void *parameter) {

	HANDLE *handles = 0;
	bool err = false;
	DWORD stat;
	int index = 0;
	char sbuffer[MAX_PATH+1];

	char dir[256];
	char drive[32];

	vector < FileChangeData* > fcd;

	// try to avoid multiple watches on the same directory.  
	unordered_set <string> watched;

	for (unordered_set<string>::iterator iter = directories.begin(); iter != directories.end(); iter++) {

		string normalized = normalize_path(iter->c_str(), true);
		if (watched.find(normalized) == watched.end())
		{
			watched.insert(normalized);
			HANDLE hdir = ::CreateFileA(normalized.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
			if (hdir != INVALID_HANDLE_VALUE) {
				FileChangeData *pf = new FileChangeData;
				pf->path = normalized;
				pf->directory_flag = 1;
				pf->fileHandle = hdir;
				pf->overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
				fcd.push_back(pf);
			}
		}
	}

	for (unordered_set<string>::iterator iter = files.begin(); iter != files.end(); iter++) {

		_splitpath_s(iter->c_str(), drive, 32, dir, 256, 0, 0, 0, 0);
		string path = drive;
		path.append(dir);
		string normalized = normalize_path(path.c_str(), true);

		if (watched.find(normalized) == watched.end())
		{
			watched.insert(normalized);
			HANDLE hdir = ::CreateFileA(normalized.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
			if (hdir != INVALID_HANDLE_VALUE) {
				FileChangeData *pf = new FileChangeData;
				pf->path = normalized;
				pf->directory_flag = 0;
				pf->fileHandle = hdir;
				pf->overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
				fcd.push_back(pf);
			}
		}
	}

	handles = new HANDLE[fcd.size() + 1];
	handles[index++] = hSignal; 

	for (vector< FileChangeData* >::iterator iter = fcd.begin(); iter != fcd.end(); iter++) {
		FileChangeData *pf = *iter;
		handles[index++] = pf->overlapped.hEvent;

		// NOT watching subtree.  see docs.
		ReadDirectoryChangesW(pf->fileHandle, pf->buffer, 1024, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE, &(pf->dwRead), &(pf->overlapped), NULL);
	}

	while (!err) {

		stat = WaitForMultipleObjects(index, handles, 0, INFINITE);
		if (stat == WAIT_OBJECT_0) {
			DebugOut(" * user exit\n");
			err = true;
		}
		else {
			DebugOut(" * stat %d\n", stat);
			unordered_set<string> changed_files;
			for (vector< FileChangeData* >::iterator iter = fcd.begin(); iter != fcd.end(); iter++) {
				DWORD dwread = 0;
				BOOL rslt = ::GetOverlappedResult((*iter)->fileHandle, &((*iter)->overlapped), &dwread, FALSE);
				if (rslt) {
					
					FileChangeData *pf = *iter;
					unsigned char *ptr = pf->buffer;
					FILE_NOTIFY_INFORMATION *pnotify = (FILE_NOTIFY_INFORMATION*)(ptr);

					while (true) {

						if (pnotify->Action == FILE_ACTION_ADDED || pnotify->Action == FILE_ACTION_MODIFIED) {

							// filename in the notify structure is not null-terminated, and the length field
							// contains the length in BYTES.  when converting use length in characters.

							int charlen = WideCharToMultiByte(CP_UTF8, NULL, pnotify->FileName, pnotify->FileNameLength / sizeof(WCHAR), sbuffer, MAX_PATH, 0, 0);
							if (charlen) {

								string str = pf->path;
								str.append(sbuffer, charlen);

								// if we're watching the directory, then any file change gets set. 
								// otherwise we need to check if we're watching this specific file.

								// in the second case, we're using the path as given by R.  that's 
								// intended to make matching easier.

								if (pf->directory_flag) changed_files.insert(str);
								else {

									for (unordered_set<string>::iterator iter = files.begin(); iter != files.end(); iter++) {
										std::string comp = *iter;
										if(!std::lexicographical_compare<std::string::iterator, std::string::iterator>(str.begin(), str.end(), comp.begin(), comp.end(), icasecomp)){
											changed_files.insert(*iter);
											break;
										}
									}
								}

							}
						}
						if (!pnotify->NextEntryOffset) break;

						// this is NOT offset in the containing memory.  it's an advance.
						// see https://msdn.microsoft.com/en-us/library/windows/desktop/aa364391(v=vs.85).aspx

						ptr += (pnotify->NextEntryOffset);
						pnotify = (FILE_NOTIFY_INFORMATION*)(ptr);
					}
					ReadDirectoryChangesW(pf->fileHandle, pf->buffer, 1024, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE, &(pf->dwRead), &(pf->overlapped), NULL);
				}
			}

			for (unordered_set<string>::iterator iter = changed_files.begin(); iter != changed_files.end(); iter++) {

				// try and open the file.  in some cases this will fail, if the editor still has a lock.
				// in that case we'll get a second event when the file is committed, so we can just ignore 
				// this one.

				HANDLE hfile = ::CreateFileA(iter->c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
				if (hfile && hfile != INVALID_HANDLE_VALUE) {
					::CloseHandle(hfile);
					changes.locked_push_back(*iter);
					DebugOut(" * file change: %s\n", iter->c_str());
				}
				else {
					DebugOut(" * still locked? open failed: %s\n", iter->c_str());
				}

			}

			if (changes.locked_size()) {
				DWORD dwID;
				CreateThread(0, 0, execFunctionThread, 0, 0, &dwID);
			}

		}

	}

	// clean up

	for (vector< FileChangeData* >::iterator iter = fcd.begin(); iter != fcd.end(); iter++) {
		FileChangeData *pf = *iter;
		CancelIo(pf->fileHandle);
		::CloseHandle(pf->overlapped.hEvent);
		::CloseHandle(pf->fileHandle);
		delete pf;
	}

	// ...

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
	directories.clear();

	for (STRVECTOR::iterator iter = list.begin(); iter != list.end(); iter++) {

		DWORD attrib = GetFileAttributesA(iter->c_str());
		if (attrib == INVALID_FILE_ATTRIBUTES) return -1;

		if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
			directories.insert(iter->c_str());
		}	
		else {
			HANDLE hfile = ::CreateFileA(iter->c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
			if (!hfile || hfile == INVALID_HANDLE_VALUE) return -1;
			::CloseHandle(hfile);
			files.insert(iter->c_str());
		}
	}

	if (directories.size() == 0 && files.size() == 0) return 0;

	DWORD dwpid = GetCurrentProcessId();
	DWORD dwtime = time(0);

	sprintf_s(szEvent, "BERT-Event-%u-%u", dwpid, dwtime);

	hSignal = ::CreateEventA(0, 0, 0, szEvent);
	hThread = ::CreateThread(0, 0, startWatchThread, szEvent, 0, &threadID);

	return 0;
}

