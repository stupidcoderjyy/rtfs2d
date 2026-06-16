//
// Created by PC on 2026/6/15.
//

#include "string_input.h"

#include "string_input.h"

#include <stdexcept>
#include <string>

namespace rtfs2d {

StringInput::StringInput(const std::string& utf8_data)
    : data_(utf8_data) {}

int StringInput::Read() {
    if (!Available()) {
        throw std::runtime_error("StringInput::Read(): no available data");
    }
    return static_cast<unsigned char>(data_[next_++]);
}

int StringInput::Forward() {
    if (!Available()) {
        throw std::runtime_error("StringInput::Forward(): no available data");
    }
    return static_cast<unsigned char>(data_[next_]);
}

void StringInput::Mark() { marks_.push_back(next_); }

void StringInput::RemoveMark() {
    if (marks_.empty()) {
        throw std::runtime_error("StringInput::RemoveMark(): no mark to remove");
    }
    marks_.pop_back();
}

void StringInput::Recover(bool consume_mark) {
    if (marks_.empty()) return;
    if (consume_mark) {
        next_ = marks_.back();
        marks_.pop_back();
    } else {
        next_ = marks_.back();
    }
}

void StringInput::Recover() { Recover(true); }

std::string StringInput::Capture() {
    if (marks_.empty()) return "";
    if (marks_.size() == 1) {
        int start = marks_.back();
        marks_.pop_back();
        return CaptureSubstring(next_, start);
    }
    // marks_.size() >= 2
    int end = marks_.back();
    marks_.pop_back();
    int start = marks_.back();
    marks_.pop_back();
    return CaptureSubstring(end, start);
}

bool StringInput::Available() const { return next_ < static_cast<int>(data_.size()); }

int StringInput::Retract() {
    if (next_ <= 0) {
        throw std::runtime_error("StringInput::Retract(): cannot retract beyond start");
    }
    --next_;
    return static_cast<unsigned char>(data_[next_]);
}

std::string StringInput::CaptureSubstring(int end, int start) const {
    if (start < 0 || end < start || end > static_cast<int>(data_.size())) {
        throw std::runtime_error("StringInput::CaptureSubstring(): invalid range");
    }
    return data_.substr(start, end - start);
}

}