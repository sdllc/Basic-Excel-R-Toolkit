
#include "DebugOut.h"
#include "RemoteShell.h"

#include "stdafx.h"
#include <Objbase.h>

#include <vector>
#include <string>

#include "util.h"
#include "json11.hpp"

#include "BERT.h"
#include "RegistryUtils.h"

#define CONNECTING_STATE 1 
#define CONNECTED_STATE 2
#define READING_STATE 4 
#define WRITING_STATE 8 
#define INSTANCES 4 
#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096


#define STATE_NULL 0

char pipename[256];

static HANDLE hThread = NULL;
static DWORD threadID = 0;
static bool threadFlag = false;

static std::vector < std::string > outboundMessages;

extern AutocompleteData autocomplete;


HANDLE hOutboundMessagesMutex = NULL;

DWORD dwState = -1;
HANDLE hPipe;
OVERLAPPED io;
BOOL pipePendingIO;

SVECTOR cmd_buffer;

extern HRESULT SafeCall(SAFECALL_CMD cmd, SVECTOR *vec, long arg, int *presult);

HWND g_HWND = NULL;
DWORD childProcessId;

BOOL CALLBACK EnumWindowsProcHide(HWND hwnd, LPARAM lParam)
{
	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);

	bool show = (bool)lParam;

	char buffer[256];
	char className[256];

	if ( pid == childProcessId )
	{
		//
		// FIXME: watch out, this could be fragile...
		//

		::RealGetWindowClassA(hwnd, className, 256);
		if (!strcmp(className, "Chrome_WidgetWin_1")) {

			GetWindowTextA(hwnd, buffer, 256);
			DebugOut("WINDOW %X: %s\n", pid, buffer);

			long style = GetWindowLong(hwnd, GWL_STYLE);
			long exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
			DebugOut("PRE: 0x%X, 0x%X\n", style, exstyle);

			if (show) {
				style |= WS_VISIBLE;    
				exstyle |= WS_EX_APPWINDOW;   
				exstyle &= ~(WS_EX_TOOLWINDOW);
			}
			else {
				style &= ~(WS_VISIBLE);
				exstyle |= WS_EX_TOOLWINDOW;
				exstyle &= ~(WS_EX_APPWINDOW);
			}

			ShowWindow(hwnd, SW_HIDE);

			SetWindowLong(hwnd, GWL_STYLE, style);
			SetWindowLong(hwnd, GWL_EXSTYLE, exstyle);

			if (show) {
				ShowWindow(hwnd, SW_SHOW);
				::SetForegroundWindow(hwnd);
			}

			style = GetWindowLong(hwnd, GWL_STYLE);
			exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
			DebugOut("POST: 0x%X, 0x%X\n", style, exstyle);

		}
	}
	return TRUE;
}

void hide_console() {

	DebugOut("Hide Console\n");
	BOOL rslt = EnumWindows(EnumWindowsProcHide, false);

}

void show_console() {

	DebugOut("Show Console\n");
	BOOL rslt = EnumWindows(EnumWindowsProcHide, true);

}


/**
 * write message, if one is on the queue.  
 */
BOOL writeNextMessage() {
	
	DWORD dwBytes;
	std::string str;

	while (true) {

		::WaitForSingleObject(hOutboundMessagesMutex, INFINITE);
		int len = outboundMessages.size();
		if (len){
			str = outboundMessages[0];
			outboundMessages.erase(outboundMessages.begin());
		}
		::ReleaseMutex(hOutboundMessagesMutex);

		if (!len) return FALSE; // nothing to write

		// DebugOut("[COMMS] writing\n");

		dwState = WRITING_STATE;
		BOOL fSuccess = ::WriteFile(hPipe, str.c_str(), str.length(), &dwBytes, &io);
		if (fSuccess && dwBytes > 0)
		{
			// DebugOut("[COMMS] immediate write success (%d)\n", dwBytes);
			dwState = CONNECTED_STATE;
			// return FALSE;
		}
		else return TRUE; // op pending

	}
}

/**
 * push a message onto the outbound list 
 */
void push_json(const json11::Json &obj) {

	std::string str = obj.dump();
	str.append("\n");

	::WaitForSingleObject(hOutboundMessagesMutex, INFINITE);
	outboundMessages.push_back(str);
	::ReleaseMutex(hOutboundMessagesMutex);
}

void handle_internal(const std::vector<json11::Json> &commands) {

	json11::Json response = json11::Json::object{
		{ "type", "response" }
	};

	int csize = commands.size();

	if (csize) {
		std::string cmd = commands[0].string_value();
		if (!cmd.compare("autocomplete") && csize == 3 ) {

			int sc, c;
			SVECTOR sv;
			sv.push_back(commands[1].string_value());
			c = commands[2].int_value();

			SafeCall(SCC_AUTOCOMPLETE, &sv, c, &sc);
			response = json11::Json::object{
				{ "type", "response" },
				{ "addition", autocomplete.addition },
				{ "comps", autocomplete.comps },
				{ "file", autocomplete.file },
				{ "function", autocomplete.function },
				{ "signature", autocomplete.signature },
				{ "token", autocomplete.token },
				{ "start", autocomplete.tokenIndex },
				{ "end", autocomplete.end }
			};

		}
		else if (!cmd.compare("set-console-width")) {

			SVECTOR sv;
			int r;
			int chars = commands[1].int_value();
			char buffer[128];

			// min/max?
			if (chars < 10) chars = 10;

			sprintf_s(buffer, 128, "options(\"width\"=%d)", chars);
			sv.push_back(std::string(buffer));
			SafeCall(SCC_CONSOLE_WIDTH, &sv, 0, &r);

		}
		else if (!cmd.compare("hide")) {
			hide_console();
		}
	}

	return push_json(response);

}

std::string buffer;

void exec(std::string line) {

	SVECTOR cmds;
	buffer += line;
	Util::split(line, '\n', 0, cmds);
	buffer = "";

	for (SVECTOR::iterator iter = cmds.begin(); iter != cmds.end(); iter++) {
		DebugOut("ITER: %s\n", iter->c_str());
		if (Util::endsWith(*iter, "}")) {

			std::string err;
			json11::Json j = json11::Json::parse(*iter, err);
			std::string jcommand = j["command"].string_value();
			if (!jcommand.compare("exec")) {

				const std::vector< json11::Json > arr = j["commands"].array_items();
				for (std::vector< json11::Json > ::const_iterator iter = arr.begin(); iter != arr.end(); iter++) {
					cmd_buffer.push_back(iter->string_value());
				}

			}
			else if (!jcommand.compare("internal")) {
				const std::vector< json11::Json > arr = j["commands"].array_items();
				return handle_internal(arr);
			}
			else DebugOut("c? %s\n", jcommand.c_str());

		}
		else {
			buffer = *iter;
		}
	}

	if (!cmd_buffer.size()) {

		json11::Json obj = json11::Json::object{
			{ "type", "exec-response" },
			{ "parsestatus", 0}
		};
		push_json(obj);

		return;
	}

	//cmd_buffer.push_back(line);
	int ips = 0;
	SafeCall(SCC_EXEC, &cmd_buffer, 0, &ips);
	DebugOut("[COMMS] safecall result: %d\n", ips);
	switch ((PARSE_STATUS_2)ips) {
	case PARSE2_INCOMPLETE:
		cmd_buffer.clear();
		break;
	case PARSE2_EXTERNAL_ERROR:
		rshell_send("processing error: is Excel busy?\n", 1);
		cmd_buffer.clear();
		break;
	case PARSE2_ERROR:
		rshell_send("parse error\n", 1);
		cmd_buffer.clear();
		break;
	default:
		cmd_buffer.clear();
		break;
	}

	json11::Json obj = json11::Json::object{
		{ "type", "exec-response" },
		{ "parsestatus", ips }
	};

	push_json(obj);
}

void endProcess() {

	json11::Json obj = json11::Json::object{
		{ "type", "control" },
		{ "message", "quit" }
	};
	push_json(obj);
	// ...

}

void startProcess() {

	char tmp[256];
	char args[512];
	char dir[512];

	// this is the home directory (R_USER or BERT_HOME)

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, args, 511, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
		ExpandEnvironmentStringsA(DEFAULT_R_USER, args, 512);
	
	SetEnvironmentVariableA("BERT_HOME", args);
	args[0] = 0;

	// FIXME: default shell path (relative to home?)

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, tmp, 255, REGISTRY_KEY, "ShellPath") || !strlen(tmp))
		strcpy_s(tmp, "BertShell");

//	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, tmp, 255, REGISTRY_KEY, "ShellArgs") || !strlen(tmp))
//		strcpy_s(tmp, "");
//	strcat_s(args, tmp);

	// FIXME: default to home dir?

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, dir, 511, REGISTRY_KEY, "ShellDir") || !strlen(dir))
		strcpy_s(dir, "");

	sprintf_s(args, "\"%s\\%s\" ", dir, tmp);

	SetEnvironmentVariableA("BERT_SHELL_HOME", dir);
	SetEnvironmentVariableA("BERT_PIPE_NAME", pipename);

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcessA(0,
		args,        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		dir[0] ? dir : NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		DebugOut("CreateProcess failed (%d).\n", GetLastError());
		return;
	}

	childProcessId = pi.dwProcessId;

}

DWORD WINAPI threadProc(LPVOID lpvParam) {

	DebugOut("[COMMS] thread starting\n");
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	bool fConnected = false;
	static char *buffer = new char[4096];
	io.hEvent = CreateEvent(0, 0, TRUE, 0);

	// create pipe
	char tmp[256];
	sprintf_s(tmp, "\\\\.\\pipe\\%s", pipename);

	hPipe = CreateNamedPipeA(
		tmp,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES, 
		4096,
		4096,
		100,
		NULL);

	if (NULL == hPipe || hPipe == INVALID_HANDLE_VALUE) {
		DebugOut("[COMMS] create pipe failed\n");
		::CoUninitialize();
		return -1;
	}

	DebugOut("[COMMS] Create pipe OK\n");

	DebugOut("[COMMS] calling startProcess\n");
	startProcess();

	DebugOut("[COMMS] calling connect (overlapped)\n");

	DWORD dwBytes, dwRead = 0, dwWrite = 0;
	bool connected = false;
	dwState = CONNECTING_STATE;

	// all these async calls _may_ return immediately, so 
	// you need to handle everything twice.

	if (ConnectNamedPipe(hPipe, &io)) {
		// this is an error
		threadFlag = false;
		DebugOut("[COMMS] ERR in connectNamedPipe");
	}
	else {
		switch (GetLastError()) {
		case ERROR_PIPE_CONNECTED: 
			SetEvent(io.hEvent);
			break;
		case ERROR_IO_PENDING:
			break;
		default:
			DebugOut("[COMMS] connect failed with %d\n", GetLastError());
			break;
		}
	}

	while (threadFlag) {

		DWORD dw = WaitForSingleObject(io.hEvent,  500);
		bool fSuccess = GetOverlappedResult(hPipe, &io, &dwBytes, 0);

		if (fSuccess) {

			DebugOut("FS: State = %d\n", dwState);

			if (dwState == CONNECTING_STATE) {
				connected = true;
				dwState = CONNECTED_STATE;
				DebugOut("[COMMS] Connecting state OK; connected\n");
			}
			else if (dwState == READING_STATE) {
				buffer[dwBytes] = 0;
				DebugOut("[COMMS] Reading state sucess (%d): ``%s''\n", dwBytes, buffer);
				exec(buffer);
			}

			if (connected) {

				if (writeNextMessage()) continue; // write op pending

				DebugOut("[COMMS] reading");

				dwState = READING_STATE;
				fSuccess = ::ReadFile(hPipe, buffer, 4095, &dwBytes, &io);
				if (fSuccess && dwBytes > 0)
				{
					buffer[dwBytes] = 0;
					DebugOut("[COMMS] immediate read success (%d): ``%s''\n", dwBytes, buffer);
					exec(buffer);


					dwState = CONNECTED_STATE;
					continue;
				}

				if (GetLastError() == ERROR_IO_PENDING) continue;
				DebugOut("[COMMS] PROBLEM\n");
			}

		} 
		else if (GetLastError() == ERROR_IO_INCOMPLETE) {

			if (connected && dwState == READING_STATE) {
				int len;
				::WaitForSingleObject(hOutboundMessagesMutex, INFINITE);
				len = outboundMessages.size();
				::ReleaseMutex(hOutboundMessagesMutex);

				if (len > 0) {
					::CancelIo(hPipe);
					if (!writeNextMessage()) {
						DebugOut("[COMMS] reading\n");

						dwState = READING_STATE;
						fSuccess = ::ReadFile(hPipe, buffer, 4095, &dwBytes, &io);
						if (fSuccess && dwBytes > 0)
						{
							buffer[dwBytes] = 0;
							DebugOut("[COMMS] immediate read success (%d): ``%s''\n", dwBytes, buffer);
							dwState = CONNECTED_STATE;
							continue;
						}

						if (GetLastError() == ERROR_IO_PENDING) continue;
						DebugOut("[COMMS] PROBLEM\n");

					}
				}
			}
		}
		else {

			DebugOut("[COMMS] !fSuccess: %d; closing, trying again\n", GetLastError());
			if (!DisconnectNamedPipe(hPipe))
			{
				DebugOut("[COMMS] DisconnectNamedPipe failed with %d.\n", GetLastError());
			}

			connected = false;
			dwState = CONNECTING_STATE;

			if (ConnectNamedPipe(hPipe, &io)) {
				// this is an error
				threadFlag = false;
				DebugOut("[COMMS] ERR in connectNamedPipe");
			}
			else {
				switch (GetLastError()) {
				case ERROR_PIPE_CONNECTED:
					SetEvent(io.hEvent);
					break;
				case ERROR_IO_PENDING:
					break;
				default:
					break;
				}
			}

		}


	}
	
	DebugOut("[COMMS] thread ending\n");
	::CoUninitialize();
	return 0;

}

int initialized = false;

void open_remote_shell() {

	if (initialized) show_console();
	else rshell_connect();

}

void rshell_connect() {

	initialized = true;

	// create the pipe name here.  we only write this once, 
	// but we may read it from different threads.

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, pipename, 63, REGISTRY_KEY, REGISTRY_VALUE_PIPE_OVERRIDE) || !strlen(pipename))
		sprintf_s(pipename, "bert-pipe-%d", GetCurrentProcessId());
	
	DebugOut("[COMMS] Create thread\n");
	threadFlag = true;
	hThread = CreateThread( NULL, 0, threadProc, 0, 0, &threadID);

	hOutboundMessagesMutex = ::CreateMutex(0, false, 0);

}

void rshell_block(bool block) {

	if (hThread && initialized) {
		std::string msg = block ? "block" : "unblock";
		json11::Json obj = json11::Json::object{
			{ "type", "control" },
			{ "data", msg }
		};
		push_json(obj);
	}

}

void rshell_disconnect() {

	if (hThread) {

		json11::Json obj = json11::Json::object{
			{ "type", "control" },
			{ "data", "quit" }
		};
		push_json(obj);

		threadFlag = false;
		WaitForSingleObject(hThread, INFINITE);
		DebugOut("[COMMS] Done\n");
		::CloseHandle(hThread);
		::CloseHandle(hOutboundMessagesMutex);
	}
	hThread = NULL;

}

void rshell_send(const char *message, int flag ) {

	std::string msg = message;

	json11::Json my_json = json11::Json::object{
		{ "type", "console" },
		{ "message", msg },
		{ "flag", flag }
	};
	std::string json_str = my_json.dump();
	json_str.append("\n");

	::WaitForSingleObject(hOutboundMessagesMutex, INFINITE);
	outboundMessages.push_back(json_str);
	::ReleaseMutex(hOutboundMessagesMutex);

}
