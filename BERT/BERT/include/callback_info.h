/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */

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

