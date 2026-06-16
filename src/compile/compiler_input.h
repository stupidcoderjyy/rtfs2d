//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_COMPILER_INPUT_H
#define RTFS2D_COMPILER_INPUT_H

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "util/buffered_input.h"
#include "compile_error.h"

namespace rtfs2d {
class CompilerInput : public BufferedInput {
public:
    // 构造函数：接受一个 ByteReader 和文件路径（可选）
    CompilerInput(std::unique_ptr<ByteReader> reader,
        std::string file_path,
        int buffer_size = kDefaultBufferSize);

    static std::unique_ptr<CompilerInput> FromString(
        const std::string& utf8_data,
        const std::string& name = "internal string",
        int buffer_size = kDefaultBufferSize);
    ~CompilerInput() override;

    void Mark() override;
    void RemoveMark() override;
    int Retract() override;
    int Read() override;
    void Recover(bool consume_mark) override;

    // 辅助方法
    std::string CurrentLine();
    CompileError ErrorAtMark(const std::string& msg);
    CompileError ErrorAtForward(const std::string& msg);
    CompileError ErrorMarkToMark(const std::string& msg);
    CompileError ErrorMarkToForward(const std::string& msg);

protected:
    void FillA() override;
    void FillB() override;

private:
    static constexpr int kReservedSize = 16;
    static constexpr int kMarkLen = 8;      // 每个标记数据存储的 int 个数
    typedef std::array<int, kMarkLen> MarkData;

    std::string file_path_;
    int row_ = 1;
    int column_ = 0;
    std::vector<int> column_sizes_;   // 每行开始的列号（行首偏移）
    std::vector<int> row_begin_;      // 每行在缓冲区中的起始位置

    // 标记数据栈：每个标记对应一个 int[kMarkLen] 数组
    std::deque<MarkData> mark_data_;

    // 内部辅助函数
    MarkData GetData() const;
    void RemoveFirstData();
    MarkData RemoveLastData();
    void RecoverFromData(const MarkData& data);
    CompileError PointError(const std::string& msg, int pos);
    CompileError RangedError(const std::string& msg, int end, int start);
};

}

#endif //RTFS2D_COMPILER_INPUT_H
