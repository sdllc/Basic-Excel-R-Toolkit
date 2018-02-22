#pragma once

#include <string>
#include <sstream>

#ifdef INCLUDE_DUMP_JSON
  #include <google\protobuf\util\json_util.h>
#endif

/**
 * common utilities for protocol buffer messages
 */
class MessageUtilities {
public:

    /**
     * unframe and return message
     */
    static bool Unframe(google::protobuf::Message &message, const char *data, uint32_t len) {
      int32_t bytes;
      memcpy(reinterpret_cast<void*>(&bytes), data, sizeof(int32_t));
      return message.ParseFromArray(data + sizeof(int32_t), bytes);
    }

    /** 
     * unframe passed string 
     */
    static bool Unframe(google::protobuf::Message &message, const std::string &message_buffer) {
      return Unframe(message, message_buffer.c_str(), (uint32_t)message_buffer.length());
    }

    /**
     * frame and return string
     */
    static std::string Frame(const google::protobuf::Message &message) {
      int32_t bytes = message.ByteSize();
      std::stringstream stream;
      stream.write(reinterpret_cast<char*>(&bytes), sizeof(int32_t));
      message.SerializeToOstream(dynamic_cast<std::ostream*>(&stream));
      return stream.str();
    }

#ifdef INCLUDE_DUMP_JSON

    /** debug/util function */
    static void DumpJSON(const google::protobuf::Message &message, const char *path = 0) {
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

};
