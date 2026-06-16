//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_ABSTRACT_INPUT_H
#define RTFS2D_ABSTRACT_INPUT_H
#include <string>

namespace rtfs2d {

class AbstractInput {
public:
    virtual ~AbstractInput() = default;
    virtual int Read() = 0;
    virtual int Forward() = 0;
    virtual std::string ReadUtf();
    virtual void Mark() = 0;
    virtual void RemoveMark() = 0;
    virtual void Recover(bool consumeMark) = 0;
    virtual void Recover();
    virtual std::string Capture() = 0;
    virtual bool Available() const = 0;
    virtual int Retract(int count);
    virtual int Retract() = 0;

    int Approach(int ch);
    int Approach(int ch1, int ch2);
    int Approach(int ch1, int ch2, int ch3) ;
    int Approach(const std::initializer_list<int> &list);
    int Find(int ch);
    int Find(int ch1, int ch2);
    int Find(int ch1, int ch2, int ch3);
    int Find(const std::initializer_list<int> &list);
    int Skip(int ch);
    int Skip(int ch1, int ch2);
    int Skip(int ch1, int ch2, int ch3);
    int Skip(const std::initializer_list<int> &list);
protected:
    bool bit_clazz_[128]{};
private:
    void PrepareBitClazz(const std::initializer_list<int>& list);
};


}



#endif //RTFS2D_ABSTRACT_INPUT_H
