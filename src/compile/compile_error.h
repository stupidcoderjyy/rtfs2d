//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_COMPILE_ERROR_H
#define RTFS2D_COMPILE_ERROR_H
#include <exception>
#include <string>

namespace rtfs2d {

class CompileError : public std::exception {
public:
    CompileError(std::string msg, int row, std::string line, std::string file_path);
    CompileError(const CompileError& other);
    CompileError(CompileError&& other) noexcept;
    CompileError& SetPos(int column);
    CompileError& SetPos(int start, int end);
    std::string FormatErrorMessage() const;
    const char* what() const noexcept override { return msg_.c_str(); }
private:
    const int row_;
    const std::string line_;
    const std::string msg_;
    const std::string file_path_;
    int start_ = 0;
    int end_ = 0;
};

}  // namespace rtfs2d

#endif //RTFS2D_COMPILE_ERROR_H
