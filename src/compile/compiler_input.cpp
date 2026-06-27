//
// Created by PC on 2026/6/15.
//

#include "compiler_input.h"

#include <algorithm>
#include <array>

#include "util/byte_reader.h"
#include "util/string_byte_reader.h"

namespace rtfs2d {

CompilerInput::CompilerInput(std::unique_ptr<ByteReader> reader, std::string file_path, int buffer_size)
        : BufferedInput(std::move(reader), buffer_size), file_path_(std::move(file_path)) {
    row_begin_.push_back(0);
    mark_data_.push_back(GetData());
}

std::unique_ptr<CompilerInput> CompilerInput::FromString(
        const std::string& utf8_data, const std::string& name, int buffer_size) {
    auto reader = std::make_unique<StringByteReader>(utf8_data);
    return std::make_unique<CompilerInput>(std::move(reader), name, buffer_size);
}

CompilerInput::~CompilerInput() {
    while (!mark_data_.empty()) {
        RemoveLastData();
    }
    // 静态池清理（可选，程序结束时自动释放）
}

void CompilerInput::Mark() {
    BufferedInput::Mark();   // 调用基类记录 marks_
    mark_data_.push_back(GetData());
}

void CompilerInput::RemoveMark() {
    BufferedInput::RemoveMark();
    RemoveLastData();
}

int CompilerInput::Retract() {
    // 如果当前 next 位置恰好是某个标记的位置，则移除该标记（保持一致性）
    if (!mark_data_.empty() && mark_data_.back()[4] == next_) {
        RemoveFirstData();
    }
    int b = BufferedInput::Retract();   // 基类实现
    switch (b) {
        case '\r':
            break;
        case '\n':
            --row_;
            if (!column_sizes_.empty()) {
                column_sizes_.pop_back();
            }
            if (!row_begin_.empty()) {
                row_begin_.pop_back();
            }
            column_ = column_sizes_.empty() ? 0 : column_sizes_.back();
            break;
        default:
            --column_;
            break;
    }
    return b;
}

int CompilerInput::Read() {
    int b = BufferedInput::Read();
    switch (b) {
        case '\n':
            ++row_;
            row_begin_.push_back(next_);
            column_sizes_.push_back(column_);
            column_ = 0;
            break;
        case '\r':
            break;
        default:
            ++column_;
            break;
    }
    return b;
}

void CompilerInput::FillA() {
    BufferedInput::FillA();
    if (fill_count_ == 1) return;
    // 移除超出 A 区间的行信息
    while (!row_begin_.empty() && row_begin_.front() < buf_end_a_) {
        row_begin_.erase(row_begin_.begin());
        if (!column_sizes_.empty()) column_sizes_.erase(column_sizes_.begin());
    }
    // 移除超出 A 区间的标记数据
    while (!mark_data_.empty() && mark_data_.front()[4] < buf_end_a_) {
        RemoveFirstData();
    }
}

void CompilerInput::FillB() {
    BufferedInput::FillB();
    // 移除超出 B 区间的行信息
    while (!row_begin_.empty() && row_begin_.front() >= buf_end_a_) {
        row_begin_.erase(row_begin_.begin());
        if (!column_sizes_.empty()) column_sizes_.erase(column_sizes_.begin());
    }
    // 移除超出 B 区间的标记数据
    while (!mark_data_.empty() && mark_data_.front()[4] >= buf_end_a_) {
        RemoveFirstData();
    }
}

std::string CompilerInput::CurrentLine() {
    int start = row_begin_.empty() ? 0 : row_begin_.back();
    Mark();
    Approach('\r');   // 假设 Approach 方法在 AbstractInput 中可用
    std::string res = CaptureSubstring(next_, start);
    Recover(true);    // 恢复并消费标记
    return res;
}

void CompilerInput::Recover(bool consume_mark) {
    BufferedInput::Recover(consume_mark);
    if (!mark_data_.empty()) {
        if (consume_mark) {
            RecoverFromData(RemoveLastData());
        } else {
            RecoverFromData(mark_data_.back());
        }
    }
}

CompileError CompilerInput::ErrorAtMark(const std::string& msg) {
    if (marks_.empty()) return ErrorAtForward(msg);
    int pos = mark_data_.back()[1];
    RemoveMark();
    return PointError(msg, pos);
}

CompileError CompilerInput::ErrorAtForward(const std::string& msg) {
    return PointError(msg, column_);
}

CompileError CompilerInput::ErrorMarkToMark(const std::string& msg) {
    if (marks_.empty()) return ErrorAtForward(msg);
    if (marks_.size() == 1) return ErrorMarkToForward(msg);
    int end = mark_data_.back()[1];
    RemoveMark();
    int start = mark_data_.back()[1];
    RemoveMark();
    return RangedError(msg, end, start);
}

CompileError CompilerInput::ErrorMarkToForward(const std::string& msg) {
    if (marks_.empty()) return ErrorAtForward(msg);
    int start = mark_data_.back()[1];
    RemoveMark();
    return RangedError(msg, column_, start);
}

std::array<int, CompilerInput::kMarkLen> CompilerInput::GetData() const {
    return {
        row_,
        std::max(0, column_),
        column_sizes_.empty() ? -1 : column_sizes_.back(),
        row_begin_.empty() ? -1 : row_begin_.back(),
        next_,
    };
}

void CompilerInput::RemoveFirstData() {
    if (!mark_data_.empty()) {
        mark_data_.pop_front();
    }
}

std::array<int, CompilerInput::kMarkLen> CompilerInput::RemoveLastData() {
    if (mark_data_.empty()) {
        return {};
    }
    auto data = mark_data_.back();
    mark_data_.pop_back();
    return data;
}

void CompilerInput::RecoverFromData(const MarkData& data) {
    row_ = data[0];
    column_ = data[1];
    if (int cs = data[2]; cs >= 0) {
        while (!column_sizes_.empty() && column_sizes_.back() != cs) {
            column_sizes_.pop_back();
        }
    } else {
        column_sizes_.clear();
    }
    int rb = data[3];
    while (!row_begin_.empty() && row_begin_.back() != rb) {
        row_begin_.pop_back();
    }
}

CompileError CompilerInput::PointError(const std::string& msg, int pos) {
    return CompileError(msg, row_, CurrentLine(), file_path_).SetPos(pos);
}

CompileError CompilerInput::RangedError(const std::string& msg, int end, int start) {
    return CompileError(msg, row_, CurrentLine(), file_path_)
        .SetPos(start, std::max(start, end));
}

} // namespace rtfs2d