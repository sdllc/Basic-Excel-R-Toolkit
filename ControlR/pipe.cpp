
#include "controlr.hpp"
#include "pipe.hpp"

Pipe::Pipe() 
	: buffer_size_(DEFAULT_BUFFER_SIZE)
	, read_buffer_(0)
    , reading_(false)
    , writing_(false)
    , connected_(false)
	//, state_(PipeState::NOT_CONNECTED) 
{
    //InitializeCriticalSectionAndSpinCount(&critical_section_, 0x400);
}

Pipe::~Pipe() {
	if (read_buffer_) delete read_buffer_;
    //DeleteCriticalSection(&critical_section_);
}

HANDLE Pipe::wait_handle_read() { return read_io_.hEvent; }

HANDLE Pipe::wait_handle_write() { return write_io_.hEvent; }

// PipeState Pipe::state() { return state_; }

DWORD Pipe::buffer_size() { return buffer_size_; }

void Pipe::QueueWrites(std::vector<std::string> &list) {
    for (auto entry : list) {
        write_stack_.push_back(entry);
    }
}

void Pipe::PushWrite(std::string &message) {
    write_stack_.push_back(message);
    NextWrite();
}

int Pipe::NextWrite() {

    DWORD bytes, result;

    if (write_stack_.size() == 0) {
        ResetEvent(write_io_.hEvent);
        writing_ = false;
        return 0; // no write
    }
    else if (writing_) {

        // if we are currently in a write operation, 
        // check if it's complete.

        result = GetOverlappedResultEx(handle_, &write_io_, &bytes, 0, FALSE);
        if (!result) {
            DWORD err = GetLastError();
            switch (err) {
            case WAIT_TIMEOUT:
            case WAIT_IO_COMPLETION:
            case ERROR_IO_INCOMPLETE:
                return 0; // write in progress
            default:
                error_ = true;
                cout << "Pipe err " << err << endl;
                return 0; // err (need a flag here)
            }
        }
    }

    // at this point we're safe to write. we can push more than
    // one write if they complete immediately

    while (write_stack_.size()) {

        ResetEvent(write_io_.hEvent);
        std::string message = write_stack_.front();
        write_stack_.pop_front();
        WriteFile(handle_, message.c_str(), (DWORD)message.length(), NULL, &write_io_);
        result = GetOverlappedResultEx(handle_, &write_io_, &bytes, 0, FALSE);
        if (result) {
            // cout << "immediate write success (" << bytes << ")" << endl;
            ResetEvent(write_io_.hEvent);
            writing_ = false;
        }
        else {
            // cout << "write pending" << endl;
            writing_ = true;
            return 0;
        }
    }

    return 1;
}

int Pipe::StartRead() {
    if (reading_ || error_ || !connected_) return 0;
    reading_ = true;
    ResetEvent(read_io_.hEvent);
	ReadFile(handle_, read_buffer_, buffer_size_, 0, &read_io_);
	return 0;
}

/*
DWORD Pipe::NonBlockingRead(std::string &buf) {

	// FIXME: what happens in message mode when the buffer is too small?

	DWORD bytes = 0;
	DWORD rslt = GetOverlappedResultEx(handle_, &read_io_, &bytes, 0, FALSE);
	if (rslt) {
		buf.assign(buffer_, bytes);
        reading_ = false;
		return 0;
	}
	else {
		DWORD err = GetLastError();
        if (err != WAIT_TIMEOUT) error_ = true;
		return err;
	}

}
*/
DWORD Pipe::Read(std::string &buf, bool block) {

    // FIXME: what happens in message mode when the buffer is too small? 
    //
    // Docs:
    // In this situation, GetLastError returns ERROR_MORE_DATA, and the client can read the 
    // remainder of the message using additional calls to ReadFile.
    // 

    // that implies we will need to hold on to the buffer.

    DWORD bytes = 0;
    DWORD success = GetOverlappedResultEx(handle_, &read_io_, &bytes, block ? INFINITE : 0, FALSE);

    if (success) {
        if (message_buffer_.length()) {
            // std::cout << "appending long mesage (" << bytes << "), this is the end" << std::endl;
            buf = message_buffer_;
            buf.append(read_buffer_, bytes);
            message_buffer_.clear();
        }
        else buf.assign(read_buffer_, bytes);
        reading_ = false;
        return 0;
    }
    else {
        DWORD err = GetLastError();
        if (err == ERROR_MORE_DATA) {
            // std::cout << "appending long mesage (" << bytes << "), this is a chunk" << std::endl;
            message_buffer_.append(read_buffer_, bytes);
            reading_ = false;
            StartRead();
            return err;
        }
        if (err != WAIT_TIMEOUT) error_ = true;
        return err;
    }

}

void Pipe::Connect() {
    connected_ = true;
    cout << "pipe connected (" << name_ << ")" << endl;
    StartRead();
}

DWORD Pipe::Reset() {

    CancelIo(handle_);
    ResetEvent(read_io_.hEvent);
    ResetEvent(write_io_.hEvent);
    DisconnectNamedPipe(handle_);

    connected_ = reading_ = writing_ = error_ = false;
    message_buffer_.clear();

    if (ConnectNamedPipe(handle_, &read_io_)) {
        cerr << "ERR in connectNamedPipe" << endl;
    }
    else {
        switch (GetLastError()) {
        case ERROR_PIPE_CONNECTED:
            SetEvent(read_io_.hEvent);
            break;
        case ERROR_IO_PENDING:
            break;
        default:
            cerr << "connect failed with " << GetLastError() << endl;
            return GetLastError();
            break;
        }
    }

    return 0;
}

DWORD Pipe::Start(std::string name, bool wait) {

	stringstream pipename;
	this->name_ = name;

	pipename << "\\\\.\\pipe\\" << name;

	handle_ = CreateNamedPipeA(
		pipename.str().c_str(),
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        MAX_PIPE_COUNT, 
		DEFAULT_BUFFER_SIZE,
		DEFAULT_BUFFER_SIZE,
		100,
		NULL);

	if (NULL == handle_ || handle_ == INVALID_HANDLE_VALUE) {
		cerr << "create pipe failed" << endl;
		return -1;
	}

	read_io_.hEvent = CreateEvent(0, TRUE, FALSE, 0);
	write_io_.hEvent = CreateEvent(0, TRUE, FALSE, 0);

	read_buffer_ = new char[buffer_size_];
	

	if (ConnectNamedPipe(handle_, &read_io_)) {
		cerr << "ERR in connectNamedPipe" << endl;
	}
	else {
		switch (GetLastError()) {
		case ERROR_PIPE_CONNECTED:
			SetEvent(read_io_.hEvent);
			break;
		case ERROR_IO_PENDING:
			break;
		default:
			cerr << "connect failed with " << GetLastError() << endl;
			return GetLastError();
			break;
		}
	}

	if (!wait) return 0;

	while (true) {

		DWORD bytes;
		DWORD rslt = GetOverlappedResultEx(handle_, &read_io_, &bytes, 1000, FALSE);

		if (rslt) {
			cout << "connected (" << name << ")" << endl;
			return 0;
		}
		else {
			DWORD err = GetLastError();
			if (err != WAIT_TIMEOUT) {
                error_ = true;
				return err;
			}
		}

	}


	return -2;
}
