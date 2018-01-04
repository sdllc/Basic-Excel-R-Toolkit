#pragma once

#include <string>

#define DEFAULT_BUFFER_SIZE (8 * 1024)

// we really only need 2 connections, except for dev/debug
#define MAX_PIPE_COUNT  4

class Pipe {

private:
	HANDLE handle_;
	OVERLAPPED read_io_;
	OVERLAPPED write_io_;
	DWORD buffer_size_;
	std::string name_;
	
    /** read buffer is a single read, limited to pipe buffer size */
    char *read_buffer_;

    /** 
     * message buffer is for messages that exceed the read buffer size, they're 
     * constructed over multiple reads
     */
    std::string message_buffer_;

    std::deque<std::string> write_stack_;

    bool connected_;
    bool reading_;
    bool writing_;
    bool error_;

public:

    /** accessor */
    bool connected() { return connected_; }

    /** accessor */
    bool reading() { return reading_; }

    /** accessor */
    bool writing() { return writing_; }

    /** accessor */
    bool error() { return error_; }

	/** create pipe, accept connection and optionally block */
    DWORD Start(std::string name, bool wait);
	
	/** we have a notification about connection, do any housekeeping */
	void Connect();

    DWORD Read(std::string &buffer, bool block = false);

    void PushWrite(std::string &message);
    void QueueWrites(std::vector<std::string> &list);
    
    /**
     * returns non-zero if data was written (without considering whether 
     * more data is avaialable); returns 0 if either no write took place,
     * or a write was started but not completed.
     */
    int NextWrite();

	int StartRead();

    DWORD Reset();

	/** accessor */
	HANDLE wait_handle_read();

	/** accessor */
	HANDLE wait_handle_write();

	/** accessor */
	DWORD buffer_size();

public:
	Pipe();
	~Pipe();

};



