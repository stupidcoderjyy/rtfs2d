//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_STRING_INPUT_H
#define RTFS2D_STRING_INPUT_H

#include <vector>

#include "abstract_input.h"

namespace rtfs2d {

class StringInput : public AbstractInput {

public:
    // 接受 UTF-8 编码的字节序列（通常来自 std::string）
    explicit StringInput(const std::string& utf8_data);
    int Read() override;
    int Forward() override;
    void Mark() override;
    void RemoveMark() override;
    void Recover(bool consume_mark) override;
    void Recover() override;  // 调用 Recover(true)
    std::string Capture() override;
    bool Available() const override;
    int Retract() override;

private:
    std::string data_;        // UTF-8 字节序列
    std::vector<int> marks_;  // 记录的位置索引
    int next_ = 0;            // 当前读取位置

    // 捕获 data_ 中 [start, end) 区间的子串
    std::string CaptureSubstring(int end, int start) const;
};

}



#endif //RTFS2D_STRING_INPUT_H
