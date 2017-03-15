
#include "DebugOut.h"
#include "RemoteShell.h"

#include "stdafx.h"
#include <Objbase.h>

#include <vector>
#include <deque>
#include <string>

#include "util.h"
#include "json11.hpp"

#include "BERT.h"
#include "RegistryUtils.h"

#include "UtilityContainers.h"

typedef enum {

	UNDEFINED = 0,

	CONNECTING,
	CONNECTED,
	READING,
	WRITING

} PIPE_STATE;

#define INSTANCES 4 
#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

/** flag: have we created the process? */
int initialized = false;

/** randomly generated pipe name */
char pipename[256];

/** flag: console is blocking. */
bool block_state = false;

/** console output buffers */
locked_string_buffer buffered_messages[2];

/** pipe read/write thread */
static HANDLE hThread = NULL;

/** control flag */
static bool threadFlag = false;

/** outbound message queue */
locked_deque<std::string> outboundMessages;

/** exec thread handle */
HANDLE hExecThread = 0;

/** queue for exec thread */
locked_deque< std::string > exec_queue;

/** event for notifying exec thread */
HANDLE hExecEvent = 0;
HANDLE hExecMutex = 0;

/** buffer for command processing */
// locked_deque < string > exec_buffer;
locked_deque < json11::Json > json_exec_buffer;

/** timer queue for pushing writes */
HANDLE hTimerQueue = 0;

/** single timer event (we can push forward) */
HANDLE hTimerQueueTimer = 0;

/** signal event for timer */
HANDLE hTimerEvent = 0;

/** "sync" callbacks */
HANDLE hSyncResponseEvent = 0;

extern std::string strresult;
extern AutocompleteData autocomplete;
extern HRESULT SafeCall(SAFECALL_CMD cmd, SVECTOR *vec, long arg, int *presult);

extern void setSyncResponse(int rslt);
extern void setSyncResponse(double rslt);

extern std::string getFunctionsDir();

PIPE_STATE pipeState = PIPE_STATE::UNDEFINED;

HANDLE hPipe;
OVERLAPPED io;
BOOL pipePendingIO;

/** PID for created child process */
DWORD childProcessId;

/** fwd: batch and push out buffered console messages */
void flushMessageBuffer();

//=============================================================================
//
// window management
//
//=============================================================================

/** 
 * tail routine for enum windows proc; hides or shows matching windows.
 */
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
		// probably match on just the Chrome_ part, should be 
		// specific enough (since we're matching PID)
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
				if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
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

/** hide console window(s) */
void hide_console() {
	DebugOut("Hide Console\n");
	EnumWindows(EnumWindowsProcHide, false);

	// FIXME: focus on the excel window

}

/** show console window(s) */
void show_console() {
	DebugOut("Show Console\n");
	EnumWindows(EnumWindowsProcHide, true);
}

//=============================================================================
//
// shell api
//
//=============================================================================

/**
 * write message, if one is on the queue.  
 */
BOOL writeNextMessage() {
	
	DWORD dwBytes;
	std::string str;

	while (true) {

		int len = outboundMessages.locked_size();
		if (!len) return FALSE; // nothing to write

		str = outboundMessages.locked_pop_front();

		pipeState = PIPE_STATE::WRITING;
		BOOL fSuccess = ::WriteFile(hPipe, str.c_str(), str.length(), &dwBytes, &io);
		if (fSuccess && dwBytes > 0)
		{
			pipeState = PIPE_STATE::CONNECTED;
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
	outboundMessages.locked_push_back(str);

	// short timer
	::ChangeTimerQueueTimer(hTimerQueue, hTimerQueueTimer, 25, 25);

}

/**
 * handle an "internal" command.  we're moving away from these in
 * favor of more generic function calls, but these are here for
 * the time being.
 */
void handle_internal(const std::vector<json11::Json> &commands) {

	json11::Json response = json11::Json::object{
		{ "type", "response" }
	};

	int csize = commands.size();

	if (csize) {

		std::string cmd = commands[0].string_value();
		DebugOut(" ** INTERNAL %s\n", cmd.c_str());

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
		else if (!cmd.compare("sync-response")) {
			DebugOut(" ** sync response\n");
			::SetEvent(hSyncResponseEvent);
		}
		else if (!cmd.compare("exec")) {

			int ips = 0;
			std::string err;
			json11::Json obj;
			SVECTOR sv;

			for (int i = 1; i < commands.size(); i++ )
				sv.push_back(commands[i].string_value());

			SafeCall(SCC_EXEC, &sv, 1, &ips);
			//DebugOut("[COMMS] safecall result: %d (%u)\n", ips, GetTickCount());
			switch ((PARSE_STATUS_2)ips) {
			case PARSE2_INCOMPLETE:
			case PARSE2_EXTERNAL_ERROR:
			case PARSE2_ERROR:
				response = json11::Json::object{
					{ "type", "error" },
					{ "error", ips }};
				break;
			default:
				obj = json11::Json::parse(strresult, err);
				if (err.length()) {
					response = json11::Json::object{
						{ "type", "error" },
						{ "error", err } };
				}
				else {
					response = json11::Json::object{
						{ "type", "response" },
						{ "response",  obj } };
				}
				break;
			}

		}
	}

	return push_json(response);

}

void exec_commands_internal(json11::Json j) {

	SVECTOR cmd_buffer;

	std::string jcommand = j["command"].string_value();
	if (!jcommand.compare("exec")) {

		const std::vector< json11::Json > arr = j["commands"].array_items();
		for (std::vector< json11::Json > ::const_iterator iter = arr.begin(); iter != arr.end(); iter++) {
			cmd_buffer.push_back(iter->string_value());
		}

	}
	else if (!jcommand.compare("internal")) {
		const std::vector< json11::Json > arr = j["commands"].array_items();

		// why does this return, instead of just processing and continuing on?
		// what happens if there are other commands on the (local) queue?

		// I guess put another way, is there a case where internal and external (exec) 
		// calls are mixed?  if not, then there's no need for the command buffer
		// to be static.  (in fact I think that's the case).

		return handle_internal(arr);
	}
	else {
		DebugOut("c? %s\n", jcommand.c_str());
	}

	if (cmd_buffer.size()) {

		int ips = 0;
		SafeCall(SCC_EXEC, &cmd_buffer, 0, &ips);

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

		flushMessageBuffer();

		push_json(json11::Json::object{
			{ "type", "exec-response" },
			{ "parsestatus", ips }
		});

	}
	else {
		push_json(json11::Json::object{
			{ "type", "exec-response" },
			{ "parsestatus", 0 }
		});
	}

}

void exec_commands(std::string line) {

	// convert to json here; we need to check them anyway

	deque < json11::Json > cmds;
	SVECTOR packets;
	Util::split(line, '\n', 0, packets);

	for (SVECTOR::iterator iter = packets.begin(); iter != packets.end(); iter++) {
		if (Util::endsWith(*iter, "}")) {

			bool consumed = false;
			std::string err;
			json11::Json j = json11::Json::parse(*iter, err);

			std::string jcommand = j["command"].string_value();
			if (!jcommand.compare("internal")) {
				const std::vector< json11::Json > arr = j["commands"].array_items();
				if (arr.size() > 0 && !arr[0].string_value().compare("sync-response" )){
					if (arr.size() > 1) {
						if (arr[1].is_number()) setSyncResponse(arr[1].number_value());
					}
					::SetEvent(hSyncResponseEvent);
					consumed = true;
				}
			}

			if( !consumed ) cmds.push_back(j);
		}
		else {
			DebugOut(" ** MALFORMED PACKET\n%s\n\n", iter->c_str());
		}
	}

	// special case: check for sync response

	// ...
	::WaitForSingleObject(hExecMutex, INFINITE);
	for (deque < json11::Json > ::iterator iter = cmds.begin(); iter != cmds.end(); iter++ )
		json_exec_buffer.locked_push_back(*iter);
	::SetEvent(hExecEvent);
	::ReleaseMutex(hExecMutex);

	// exec_commands_internal(line);
}

void flushMessageBuffer() {

	string s;

	// send buffer 1, then buffer 0
	// wait: how does that make any sense? what's up with the two buffers, anyway?

	for (int i = 1; i >= 0; i--) {
		buffered_messages[i].take(s);
		if (s.length()) {
			push_json(json11::Json::object{
				{ "type", "console" }, { "message", s }, { "flag", i }
			});
		}
	}

}

/**
 * start the console process 
 */
void startProcess() {

	//char args[1024];
	//char tmp[256];
	//char dir[512];

	char *args = new char[1024];
	char *dir = new char[512];
	char *tmp = new char[256];
	char *shell_args = new char[256];

	// install dir (for locating files)

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, args, 511, REGISTRY_KEY, REGISTRY_VALUE_INSTALL_DIR))
		strcpy_s(args, 1024, "");

	SetEnvironmentVariableA("BERT_INSTALL", args);
	args[0] = 0;

	// this is the home directory (R_USER or BERT_HOME)

	if (!CRegistryUtils::GetRegExpandString(HKEY_CURRENT_USER, args, 511, REGISTRY_KEY, REGISTRY_VALUE_R_USER))
		ExpandEnvironmentStringsA(DEFAULT_R_USER, args, 512);
	
	SetEnvironmentVariableA("BERT_HOME", args);
	args[0] = 0;

	// startup folder (in documents by default)

	std::string strStartupFolder = getFunctionsDir();
	SetEnvironmentVariableA("BERT_FUNCTIONS_DIRECTORY", strStartupFolder.c_str());

	// FIXME: default shell path (relative to home?)

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, tmp, 255, REGISTRY_KEY, "ShellPath") || !strlen(tmp))
		strcpy_s(tmp, 256, "BertShell");

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, shell_args, 255, REGISTRY_KEY, "ShellArgs") || !strlen(shell_args))
		strcpy_s(shell_args, 256, "");

	// FIXME: default to home dir?

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, dir, 511, REGISTRY_KEY, "ShellDir") || !strlen(dir))
		strcpy_s(dir, 256, "");

	sprintf_s(args, 1024, "\"%s\\%s\" %s", dir, tmp, shell_args);

	// set env vars for child process

	SetEnvironmentVariableW(L"BERT_VERSION", BERT_VERSION);
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
	}
	else {
		childProcessId = pi.dwProcessId;
	}

	delete[] args;
	delete[] dir;
	delete[] tmp;
	delete[] shell_args;

}

DWORD WINAPI execThreadProc(LPVOID lpvParam) {

	DebugOut("[EXEC] thread starting\n");

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	deque< json11::Json > buffer;

	while (threadFlag) {
		DWORD dw = ::WaitForSingleObject(hExecEvent, 500);
		if (dw == WAIT_OBJECT_0) {

			::WaitForSingleObject(hExecMutex, INFINITE);
			json_exec_buffer.locked_consume(buffer);
			::ResetEvent(hExecEvent);
			::ReleaseMutex(hExecMutex);

			for (deque<json11::Json>::iterator iter = buffer.begin(); iter != buffer.end(); iter++ )
				exec_commands_internal(*iter);

			buffer.clear();

		}
	}

	// done 
	::CoUninitialize();

	DebugOut("[EXEC] thread done\n");
	return 0;
}

/**
 * timer proc
 */
VOID CALLBACK timerProc(PVOID lpParam, BOOLEAN TimerOrWaitFired) {

	if (!threadFlag) return; // closing 

	// signal io
	SetEvent(hTimerEvent);

	// reset to slow timer
	ChangeTimerQueueTimer(hTimerQueue, hTimerQueueTimer, 1000, 1000);
}

/**
 * pipe thread proc
 */
DWORD WINAPI threadProc(LPVOID lpvParam) {

	DebugOut("[COMMS] thread starting\n");
	
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	bool fConnected = false;
	static char *buffer = new char[4096];
	io.hEvent = CreateEvent(0, 0, TRUE, 0);

	// create pipe
	char tmp[256];
	sprintf_s(tmp, "\\\\.\\pipe\\%s", pipename);

	// re: buffer sizes, from docs:
	//
	// The input and output buffer sizes are advisory.The actual buffer size reserved for each end of the 
	// named pipe is either the system default, the system minimum or maximum, or the specified size rounded 
	// up to the next allocation boundary.The buffer size specified should be small enough that your process 
	// will not run out of nonpaged pool, but large enough to accommodate typical requests.
	//

	hPipe = CreateNamedPipeA(
		tmp,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		8, // PIPE_UNLIMITED_INSTANCES, // we really only need 1, except for dev/debug
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
	pipeState = PIPE_STATE::CONNECTING;

	// all these async calls _may_ return immediately, so you need to handle 
	// everything twice. [really? how does that make any sense?]

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

	HANDLE handles[2];
	handles[1] = hTimerEvent;
	handles[0] = io.hEvent;

	while (threadFlag) {

		DWORD dw = WaitForMultipleObjects(2, handles, FALSE, 500);
		ResetEvent(hTimerEvent);

		//DWORD dw = WaitForSingleObject(io.hEvent,  500);

		bool fSuccess = GetOverlappedResult(hPipe, &io, &dwBytes, 0);

		flushMessageBuffer();

		// /message buffering

		if (fSuccess) {

			// DebugOut("FS: State = %d\n", pipeState);

			if (pipeState == PIPE_STATE::CONNECTING) {
				connected = true;
				pipeState = PIPE_STATE::CONNECTED;
				DebugOut("[COMMS] Connecting state OK; connected\n");
			}
			else if (pipeState == PIPE_STATE::READING) {
				buffer[dwBytes] = 0;
				DebugOut("[COMMS] Reading state sucess (%d): ``%s''\n", dwBytes, buffer);
				exec_commands(buffer);
			}

			if (connected) {

				if (writeNextMessage()) continue; // write op pending

				DebugOut("[COMMS] reading");

				pipeState = PIPE_STATE::READING;
				fSuccess = ::ReadFile(hPipe, buffer, 4095, &dwBytes, &io);
				if (fSuccess && dwBytes > 0)
				{
					buffer[dwBytes] = 0;
					DebugOut("[COMMS] immediate read success (%d): ``%s''\n", dwBytes, buffer);
					exec_commands(buffer);
					pipeState = PIPE_STATE::CONNECTED;
					continue;
				}

				if (GetLastError() == ERROR_IO_PENDING) continue;
				DebugOut("[COMMS] PROBLEM\n");
			}

		} 
		else if (GetLastError() == ERROR_IO_INCOMPLETE) {

			if (connected && pipeState == PIPE_STATE::READING) {
				size_t len = outboundMessages.locked_size();
				if (len > 0) {
					::CancelIo(hPipe);
					if (!writeNextMessage()) {
						DebugOut("[COMMS] reading\n");

						pipeState = PIPE_STATE::READING;
						fSuccess = ::ReadFile(hPipe, buffer, 4095, &dwBytes, &io);
						if (fSuccess && dwBytes > 0)
						{
							buffer[dwBytes] = 0;
							DebugOut("[COMMS] immediate read success (%d): ``%s''\n", dwBytes, buffer);
							exec_commands(buffer);
							pipeState = PIPE_STATE::CONNECTED;
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
			pipeState = PIPE_STATE::CONNECTING;

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

/**
 * open the console, either creating a new process or showing the window.
 */
void open_remote_shell() {

	// if it's already open (but hidden), just show
	if (initialized) return show_console();

	// set flag
	initialized = true;

	// create the pipe name here.  we only write this once, but we may read it 
	// from different threads.  possible to override in registry for debug (seems 
	// like overkill)

	if (!CRegistryUtils::GetRegString(HKEY_CURRENT_USER, pipename, 63, REGISTRY_KEY, REGISTRY_VALUE_PIPE_OVERRIDE) || !strlen(pipename))
		sprintf_s(pipename, "bert-pipe-%d", GetCurrentProcessId());
	
	DebugOut("[COMMS] Create thread\n");
	
	// control flag
	threadFlag = true;

	// we don't care about ids
	DWORD id;

	// create event for exec
	hExecEvent = ::CreateEvent(0, TRUE, FALSE, 0);
	hExecMutex = ::CreateMutex(0, FALSE, 0);

	hSyncResponseEvent = ::CreateEvent(0, TRUE, FALSE, 0);

	// timer event (signal)
	hTimerEvent = ::CreateEvent(0, TRUE, FALSE, 0);

	// start exec thread
	hExecThread = CreateThread(NULL, 0, execThreadProc, 0, 0, &id);

	// start
	hThread = CreateThread( NULL, 0, threadProc, 0, 0, &id);

	// timer queue and timer.  1 second period.
	hTimerQueue = CreateTimerQueue();
	CreateTimerQueueTimer(&hTimerQueueTimer, hTimerQueue, timerProc, 0, 1000, 1000, 0);
}

/**
 * block the shell, meaning don't allow processing (or unblock a 
 * blocked shell).  this is done when R is running in another context. 
 */
void rshell_block(bool block) {

	if (block_state == block) return; // don't double up

	if (!block) flushMessageBuffer();

	block_state = block;
	std::string msg = block ? "block" : "unblock";
	push_json(json11::Json::object{
		{ "type", "control" },
		{ "data", msg }
	});

}

/**
 * disconnect and shut down
 */
void rshell_disconnect() {

	initialized = false;

	if (hThread) {

		push_json(json11::Json::object{
			{ "type", "control" },
			{ "data", "quit" }
		});

		threadFlag = false;

		WaitForSingleObject(hThread, INFINITE);
		::CloseHandle(hThread);
		DebugOut("[COMMS] Done\n");

		WaitForSingleObject(hExecThread, INFINITE);
		::CloseHandle(hExecThread);
		DebugOut("[EXEC] Done\n");

		// NOTE: timer queue and timer queue timer handles are 
		// closed automatically by this function, so don't close them

		::DeleteTimerQueueEx(hTimerQueue, INVALID_HANDLE_VALUE);

		::CloseHandle(hTimerEvent);
		::CloseHandle(hSyncResponseEvent);

		::CloseHandle(hExecEvent);
		::CloseHandle(hExecMutex);
	}

	hThread = NULL;
}

/**
 * push packet.  for now, the packet may be json text.  FIXME.
 */
void rshell_push_packet(const char *channel, const char *data, bool wait) {
	push_json(json11::Json::object{
		{ "type", "push" },
		{ "channel", channel },
		{ "data", data }
	});

	if (!wait) return;
	::WaitForSingleObject(hSyncResponseEvent, INFINITE );
	::ResetEvent(hSyncResponseEvent);

}

/**
 * push text for the console.  because we receive tiny text chunks,
 * we do a little but of buffering into larger packets.
 */
void rshell_send(const char *message, int flag ) {

	static int last_flag = 0;

	// buffering doesn't make much sense if we get messages out of 
	// order.  this should be rare, as flagged messages tend to be 
	// rare.  if the flag toggles, then push out any pending data.

	if (flag != last_flag) flushMessageBuffer();
	last_flag = flag;

	buffered_messages[flag ? 1 : 0].append(message);

}

