//
// Created by PC on 2026/6/15.
//

#include "string_byte_reader.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace rtfs2d {

StringByteReader::StringByteReader(const std::string& utf8_data)
    : data_(utf8_data), next_(0) {}

int StringByteReader::Read(char* dest, int start, int length) {
    if (dest == nullptr || start < 0 || length <= 0) {
        return 0;
    }
    int available = static_cast<int>(data_.size()) - next_;
    int to_read = std::min(length, available);
    if (to_read > 0) {
        std::memcpy(dest + start, data_.data() + next_, to_read);
        next_ += to_read;
    }
    return to_read;
}

int StringByteReader::ReadByte() {
    throw std::runtime_error("StringByteReader::ReadByte not implemented");
}

int StringByteReader::ReadInt() {
    throw std::runtime_error("StringByteReader::ReadInt not implemented");
}

long long StringByteReader::ReadLong() {
    throw std::runtime_error("StringByteReader::ReadLong not implemented");
}

double StringByteReader::ReadDouble() {
    throw std::runtime_error("StringByteReader::ReadDouble not implemented");
}

float StringByteReader::ReadFloat() {
    throw std::runtime_error("StringByteReader::ReadFloat not implemented");
}

bool StringByteReader::ReadBool() {
    throw std::runtime_error("StringByteReader::ReadBool not implemented");
}

short StringByteReader::ReadShort() {
    throw std::runtime_error("StringByteReader::ReadShort not implemented");
}

std::string StringByteReader::ReadString() {
    // 简化实现，如需要可后续完善
    throw std::runtime_error("StringByteReader::ReadString not implemented");
}

bool StringByteReader::Available() {
    return next_ < static_cast<int>(data_.size());
}

}  // namespace rtfs2d
