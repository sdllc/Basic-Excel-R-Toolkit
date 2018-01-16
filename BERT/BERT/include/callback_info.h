#pragma once

#include "stdafx.h"
#include "variable.pb.h"

class CallbackInfo {
public:
  HANDLE default_unsignaled_event_;
  HANDLE default_signaled_event_;
  BERTBuffers::CallResponse callback_call_;
  BERTBuffers::CallResponse callback_response_;

public:
  CallbackInfo() {
    default_signaled_event_ = CreateEvent(0, TRUE, TRUE, 0);
    default_unsignaled_event_ = CreateEvent(0, TRUE, FALSE, 0);
  }

  ~CallbackInfo() {
    CloseHandle(default_signaled_event_);
    CloseHandle(default_unsignaled_event_);
  }

};

