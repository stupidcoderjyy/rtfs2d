//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_BUFFERED_INPUT_H
#define RTFS2D_BUFFERED_INPUT_H

#include <memory>
#include <vector>

#include "abstract_input.h"
#include "byte_reader.h"

namespace rtfs2d {

// 带双缓冲区的字节流输入，支持标记、回退等操作
class BufferedInput : public AbstractInput {
public:
    static constexpr int kDefaultBufferSize = 2048;
    static constexpr int kMaxBufferSize = 4096;
    // 接受一个外部 ByteReader，并可选缓冲区大小
    explicit BufferedInput(std::unique_ptr<ByteReader> reader, int buffer_size = kDefaultBufferSize);
    static std::unique_ptr<BufferedInput> FromString(const std::string& utf8_data, int buffer_size = kDefaultBufferSize);

    ~BufferedInput() override = default;

    // AbstractInput 接口实现
    void Mark() override;
    void RemoveMark() override;
    void Recover(bool consume_mark) override;
    void Recover() override;  // 调用 Recover(true)
    int Retract() override;
    int Retract(int count) override;
    bool Available() const override;
    int Read() override;
    int Forward() override;
    std::string Capture() override;
protected:
    std::vector<int> marks_;                // 标记位置栈
    std::vector<char> buffer_;              // 双缓冲区（大小 2 * buffer_size）
    int buffer_size_ = 0;                   // 单个缓冲区大小（A和B各半）
    int buf_end_a_ = 0;                     // 缓冲区A的结束索引（大小 = buffer_size_）
    int buf_end_b_ = 0;                     // 缓冲区B的结束索引（大小 = 2 * buffer_size_）
    int input_end_ = -1;                    // 整个输入的末尾偏移，-1 表示未到达末尾
    int next_ = 0;                          // 当前读取位置
    int fill_count_ = 0;                    // 填充次数（奇偶用于切换A/B）

    virtual void FillA();                   // 填充缓冲区A
    virtual void FillB();                   // 填充缓冲区B
    std::string CaptureSubstring(int end, int start) const;  // 捕获区间 [start, end)
private:
    std::unique_ptr<ByteReader> reader_;    // 底层字节读取器

    void FillAInternal();                   // 内部实际填充A的实现
    void MarkInternal();                    // 内部标记实现
};

}  // namespace rtfs2d

#endif //RTFS2D_BUFFERED_INPUT_H
