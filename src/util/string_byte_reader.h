//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_STRING_BYTE_READER_H
#define RTFS2D_STRING_BYTE_READER_H

#include <string>

#include "byte_reader.h"

namespace rtfs2d {

// 基于 UTF-8 字符串的字节读取器
class StringByteReader final : public ByteReader {
public:
    // 接受 UTF-8 编码的字符串数据
    explicit StringByteReader(const std::string& utf8_data);

    // ByteReader 接口实现
    int Read(char* dest, int start, int length) override;
    int ReadByte() override;
    int ReadInt() override;
    long long ReadLong() override;
    double ReadDouble() override;
    float ReadFloat() override;
    bool ReadBool() override;
    short ReadShort() override;
    std::string ReadString() override;
    bool Available() override;

private:
    std::string data_;  // UTF-8 字节序列
    int next_ = 0;      // 当前读取位置
};

}  // namespace rtfs2d

#endif //RTFS2D_STRING_BYTE_READER_H
