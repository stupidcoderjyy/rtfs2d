//
// Created by PC on 2026/6/15.
//

#include "buffered_input.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "string_byte_reader.h"

namespace rtfs2d {

BufferedInput::BufferedInput(std::unique_ptr<ByteReader> reader, int buffer_size)
    : buffer_size_(buffer_size), reader_(std::move(reader)) {
    if (buffer_size_ <= 0 || buffer_size_ > kMaxBufferSize) {
        throw std::runtime_error("BufferedInput: invalid buffer size");
    }
    if (reader_ == nullptr) {
        throw std::runtime_error("BufferedInput: null reader");
    }
    buffer_.resize(2 * buffer_size_);
    buf_end_a_ = buffer_size_;
    buf_end_b_ = 2 * buffer_size_;
    FillAInternal();
    MarkInternal();
}

std::unique_ptr<BufferedInput> BufferedInput::FromString(const std::string& utf8_data, int buffer_size) {
    return std::make_unique<BufferedInput>(std::make_unique<StringByteReader>(utf8_data), buffer_size);
}

bool BufferedInput::Available() const {
    return input_end_ < 0 || next_ != input_end_;
}

int BufferedInput::Read() {
    if (!Available()) {
        throw std::runtime_error("BufferedInput::Read: not available");
    }
    char result = buffer_[next_++];
    if (next_ == buf_end_b_) {
        next_ = 0;
        if ((fill_count_ & 1) == 0) {
            FillAInternal();
        }
    } else if (next_ == buf_end_a_) {
        if ((fill_count_ & 1) == 1) {
            FillB();
        }
    }
    return static_cast<unsigned char>(result);
}

int BufferedInput::Forward() {
    if (!Available()) {
        throw std::runtime_error("BufferedInput::Forward: not available");
    }
    return static_cast<unsigned char>(buffer_[next_]);
}

void BufferedInput::FillA() {
    FillAInternal();
}

void BufferedInput::FillAInternal() {
    ++fill_count_;
    if (int size = reader_->Read(buffer_.data(), 0, buf_end_a_); size < buf_end_a_) {
        input_end_ = size;
    }
    // 移除所有位于当前A区间的标记
    auto it = marks_.begin();
    while (it != marks_.end() && *it < buf_end_a_) {
        it = marks_.erase(it);
    }
}

void BufferedInput::FillB() {
    ++fill_count_;
    if (int size = reader_->Read(buffer_.data(), buf_end_a_, buf_end_a_); size < buf_end_a_) {
        input_end_ = buf_end_a_ + size;
    }
    // 移除所有位于当前B区间的标记
    auto it = marks_.begin();
    while (it != marks_.end() && *it >= buf_end_a_) {
        it = marks_.erase(it);
    }
}

std::string BufferedInput::CaptureSubstring(int end, int start) const {
    if (start == end) {
        return {};
    }
    if (start < end) {
        // 连续区间，直接构造
        return {buffer_.data() + start, static_cast<std::string::size_type>(end - start)};
    }
    // 回绕区间：从 start 到缓冲区末尾，再从开头到 end
    int len_b = buf_end_b_ - start;
    int total_len = len_b + end;
    std::string result;
    result.reserve(total_len);
    result.append(buffer_.data() + start, len_b);
    result.append(buffer_.data(), end);
    return result;
}

void BufferedInput::Mark() {
    MarkInternal();
}

void BufferedInput::MarkInternal() {
    marks_.push_back(next_);
}

void BufferedInput::RemoveMark() {
    if (marks_.empty()) {
        throw std::runtime_error("BufferedInput::RemoveMark: no mark");
    }
    marks_.pop_back();
}

void BufferedInput::Recover(bool consume_mark) {
    if (marks_.empty()) return;
    if (consume_mark) {
        next_ = marks_.back();
        marks_.pop_back();
    } else {
        next_ = marks_.back();
    }
}

void BufferedInput::Recover() {
    Recover(true);
}

int BufferedInput::Retract() {
    if (buffer_.empty()) {
        throw std::runtime_error("BufferedInput::Retract: closed");
    }
    if (next_ == 0) {
        if (fill_count_ == 1 || (fill_count_ & 1) == 0) {
            throw std::runtime_error("BufferedInput::Retract: exceed retract limit");
        }
        next_ = buf_end_b_ - 1;
    } else if (next_ == buf_end_a_) {
        if ((fill_count_ & 1) == 1) {
            throw std::runtime_error("BufferedInput::Retract: exceed retract limit");
        }
        --next_;
    } else {
        --next_;
    }
    return static_cast<unsigned char>(buffer_[next_]);
}

int BufferedInput::Retract(int count) {
    return AbstractInput::Retract(count);
}

std::string BufferedInput::Capture() {
    if (buffer_.empty()) {
        throw std::runtime_error("BufferedInput::Capture: closed");
    }
    if (marks_.empty()) return "";
    if (marks_.size() == 1) {
        int start = marks_.back();
        RemoveMark();
        return CaptureSubstring(next_, start);
    }
    // marks_.size() >= 2
    int end = marks_.back();
    RemoveMark();
    int start = marks_.back();
    RemoveMark();
    return CaptureSubstring(end, start);
}

}  // namespace rtfs2d