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
 
#include "message_utilities.h"

namespace MessageUtilities {
  
  TypeFlags CheckArrayType(const BERTBuffers::Array &arr, bool allow_nil, bool allow_missing) {
    TypeFlags result = (TypeFlags::integer | TypeFlags::real | TypeFlags::numeric | TypeFlags::string | TypeFlags::logical);
    int length = arr.data_size();
    for (int i = 0; result && i < length; i++) {
      const auto &element = arr.data(i);
      switch (element.value_case()) {
      case BERTBuffers::Variable::ValueCase::kStr:
        result = result & (TypeFlags::string);
        break;
      case BERTBuffers::Variable::ValueCase::kInteger:
        result = result & (TypeFlags::integer | TypeFlags::numeric);
        break;
      case BERTBuffers::Variable::ValueCase::kReal:
        result = result & (TypeFlags::real | TypeFlags::numeric);
        break;
      case BERTBuffers::Variable::ValueCase::kBoolean:
        result = result & (TypeFlags::logical);
        break;
      case BERTBuffers::Variable::ValueCase::kMissing:
        if (!allow_missing) return TypeFlags::nil;
        break;
      case BERTBuffers::Variable::ValueCase::kNil:
        if (!allow_nil) return TypeFlags::nil;
        break;
      default:
        return TypeFlags::nil;
      }
    }
    return result;
  }
  
  bool Unframe(google::protobuf::Message &message, const char *data, uint32_t len) {
    int32_t bytes;
    memcpy(reinterpret_cast<void*>(&bytes), data, sizeof(int32_t));
    return message.ParseFromArray(data + sizeof(int32_t), bytes);
  }

  bool Unframe(google::protobuf::Message &message, const std::string &message_buffer) {
    return Unframe(message, message_buffer.c_str(), (uint32_t)message_buffer.length());
  }
  
  std::string Frame(const google::protobuf::Message &message) {
    int32_t bytes = message.ByteSize();
    std::stringstream stream;
    stream.write(reinterpret_cast<char*>(&bytes), sizeof(int32_t));
    message.SerializeToOstream(dynamic_cast<std::ostream*>(&stream));
    return stream.str();
  }

#ifdef INCLUDE_DUMP_JSON

  /** debug/util function */
  void DumpJSON(const google::protobuf::Message &message, const char *path ) {
    std::string str;
    google::protobuf::util::JsonOptions opts;
    opts.add_whitespace = true;
    google::protobuf::util::MessageToJsonString(message, &str, opts);
    if (path) {
      FILE *f;
      fopen_s(&f, path, "w");
      if (f) {
        fwrite(str.c_str(), sizeof(char), str.length(), f);
        fflush(f);
      }
      fclose(f);
    }
    else std::cout << str << std::endl;
  }

#endif

}