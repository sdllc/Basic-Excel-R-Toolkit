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

#include <string>
#include <sstream>

#include "variable.pb.h"

// #define INCLUDE_DUMP_JSON

#ifdef INCLUDE_DUMP_JSON
#include <google\protobuf\util\json_util.h>
#endif

/**
 * common utilities for protocol buffer messages
 */

namespace MessageUtilities {

  typedef enum {
    nil = 0x00,
    integer = 0x01,
    real = 0x02,
    numeric = 0x04,
    string = 0x08,
    logical = 0x10
  }
  TypeFlags;

  inline TypeFlags operator | (TypeFlags a, TypeFlags b) {
    return static_cast<TypeFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
  }

  inline TypeFlags operator & (TypeFlags a, TypeFlags b) {
    return static_cast<TypeFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
  }

  /**
   * check if an array is a single type, allowing nulls and missing values.
   * the "numeric" type means it's only numeric but has a mix of integers and
   * real/float values, so you probably want to reduce to real.
   */
  TypeFlags CheckArrayType(const BERTBuffers::Array &arr, bool allow_nil = true, bool allow_missing = true);

  /**
   * unframe and return message
   */
  bool Unframe(google::protobuf::Message &message, const char *data, uint32_t len);

  /**
   * unframe passed string
   */
  bool Unframe(google::protobuf::Message &message, const std::string &message_buffer);

  /**
   * frame and return string
   */
  std::string Frame(const google::protobuf::Message &message);
  
#ifdef INCLUDE_DUMP_JSON

  /** debug/util function */
  void DumpJSON(const google::protobuf::Message &message, const char *path = 0);

#endif


};
