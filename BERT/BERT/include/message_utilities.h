#pragma once

#include <string>
#include <sstream>
#include <google\protobuf\util\json_util.h>

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
     * frame and return string
     */
    static std::string Frame(const google::protobuf::Message &message) {
        int32_t bytes = message.ByteSize();
        std::stringstream stream;
        stream.write(reinterpret_cast<char*>(&bytes), sizeof(int32_t));
        message.SerializeToOstream(dynamic_cast<std::ostream*>(&stream));
        return stream.str();
    }

};
