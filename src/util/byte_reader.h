//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_BYTE_ReadER_H
#define RTFS2D_BYTE_ReadER_H
#include <string>

namespace rtfs2d {

class ByteReader {
public:
    virtual int Read(char* dest, int start, int length) = 0;
    virtual int ReadByte() = 0;
    virtual int ReadInt() = 0;
    virtual long long ReadLong() = 0;
    virtual double ReadDouble() = 0;
    virtual float ReadFloat() = 0;
    virtual bool ReadBool() = 0;
    virtual short ReadShort() = 0;
    virtual std::string ReadString() = 0;
    virtual bool Available() = 0;
    virtual ~ByteReader() = default;
};

}

#endif //RTFS2D_BYTE_ReadER_H
