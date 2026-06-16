//
// Created by PC on 2026/6/15.
//

#include "compile_error.h"

#include <iostream>
#include <string>
#include <spdlog/spdlog.h>

namespace rtfs2d {

CompileError::CompileError(std::string msg, int row, std::string line, std::string file_path)
: row_(row), line_(std::move(line)), msg_(std::move(msg)), file_path_(std::move(file_path)) {}

CompileError::CompileError(const CompileError& other)
    : row_(other.row_),
      line_(other.line_),
      msg_(other.msg_),
      file_path_(other.file_path_),
      start_(other.start_),
      end_(other.end_) {}

CompileError::CompileError(CompileError&& other) noexcept
    : row_(other.row_),
      line_(std::move(other.line_)),
      msg_(std::move(other.msg_)),
      file_path_(std::move(other.file_path_)),
      start_(other.start_),
      end_(other.end_) {}

CompileError& CompileError::SetPos(int column) {
    start_ = column;
    end_ = column;
    return *this;
}

CompileError& CompileError::SetPos(int start, int end) {
    start_ = start;
    end_ = end;
    return *this;
}

std::string CompileError::FormatErrorMessage() const {
    return std::format("{}:{}:{}: {}\n    {}\n{}{}",
        file_path_, row_, start_, msg_, line_,
        std::string(start_ + 4, ' '),
        std::string(end_ - start_ + 1, '^'));
}
}
