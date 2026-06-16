//
// Created by PC on 2026/6/15.
//

#include "abstract_input.h"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace rtfs2d {
std::string AbstractInput::ReadUtf() {
    switch (int b1 = Read() & 0xFF; b1 >> 4) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7: {
            char data[2]{static_cast<char>(b1), '\0'};
            return {data};
        }
        case 12:
        case 13: {
            int b2 = Read() & 0xFF;
            if ((b2 & 0xC0) != 0x80) {
                std::ostringstream oss;
                oss << "Malformed UTF-8: invalid continuation byte after 0x"
                    << std::hex << b1;
                throw std::runtime_error(oss.str());
            }
            char data[3]{static_cast<char>(b1), static_cast<char>(b2), '\0'};
            return {data};
        }
        case 14: {
            int b2 = Read() & 0xFF;
            int b3 = Read() & 0xFF;
            if ((b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) {
                std::ostringstream oss;
                oss << "Malformed UTF-8: invalid continuation bytes after 0x"
                    << std::hex << b1;
                throw std::runtime_error(oss.str());
            }
            char data[4]{static_cast<char>(b1), static_cast<char>(b2), static_cast<char>(b3), '\0'};
            return {data};
        }
        default:
            std::ostringstream oss;
            oss << "Malformed UTF-8: invalid leading byte 0x" << std::hex << b1;
            throw std::runtime_error(oss.str());
    }
}

void AbstractInput::Recover() {
    Recover(true);
}

int AbstractInput::Retract(int count) {
    if (count <= 0) {
        return -1;
    }
    count--;
    for (int i = 0 ; i < count ; i ++) {
        Retract();
    }
    return Retract();
}


int AbstractInput::Approach(int ch) {
    while (Available()) {
        if (Forward() == ch) {
            return ch;
        }
        Read();
    }
    return -1;
}

int AbstractInput::Approach(int ch1, int ch2) {
    while (Available()) {
        if (Forward() == ch1) {
            return ch1;
        }
        if (Forward() == ch2) {
            return ch2;
        }
        Read();
    }
    return -1;
}

int AbstractInput::Approach(int ch1, int ch2, int ch3) {
    while (Available()) {
        int ch = Forward();
        if (ch == ch1) {
            return ch1;
        }
        if (ch == ch2) {
            return ch2;
        }
        if (ch == ch3) {
            return ch3;
        }
        Read();
    }
    return -1;
}

void prepareBitClazz(bool* clazz, const std::initializer_list<int>& list) {
    std::memset(clazz, false, 128);
    for (const auto &ch : list) {
        if (ch >= 0 && ch < 128) {
            clazz[ch] = true;
        }
    }
}

int AbstractInput::Approach(const std::initializer_list<int>& list) {
    PrepareBitClazz(list);
    while (Available()) {
        int ch = Forward();
        if (ch >= 0 && bit_clazz_[ch]) {
            return ch;
        }
        Read();
    }
    return -1;
}

int AbstractInput::Find(int ch) {
    while (Available()) {
        if (Read() == ch) {
            return ch;
        }
    }
    return -1;
}

int AbstractInput::Find(int ch1, int ch2) {
    while (Available()) {
        int cur = Read();
        if (cur == ch1 || cur == ch2) {
            return cur;
        }
    }
    return -1;
}

int AbstractInput::Find(int ch1, int ch2, int ch3) {
    while (Available()) {
        int cur = Read();
        if (cur == ch1 || cur == ch2 || cur == ch3) {
            return cur;
        }
    }
    return -1;
}

int AbstractInput::Find(const std::initializer_list<int> &list) {
    PrepareBitClazz(list);
    while (Available()) {
        int ch = Read();
        if (ch >= 0 && bit_clazz_[ch]) {
            return ch;
        }
        Read();
    }
    return -1;
}

int AbstractInput::Skip(int ch) {
    int pre = -1;
    while (Available()) {
        int b = Read();
        if (b != ch) {
            Retract();
            break;
        }
        pre = b;
    }
    return pre;
}

int AbstractInput::Skip(int ch1, int ch2) {
    int pre = -1;
    while (Available()) {
        int b = Read();
        if (b != ch1 && b != ch2) {
            Retract();
            break;
        }
        pre = b;
    }
    return pre;
}

int AbstractInput::Skip(int ch1, int ch2, int ch3) {
    int pre = -1;
    while (Available()) {
        int b = Read();
        if (b != ch1 && b != ch2 && b != ch3) {
            Retract();
            break;
        }
        pre = b;
    }
    return pre;
}

int AbstractInput::Skip(const std::initializer_list<int> &list) {
    PrepareBitClazz(list);
    int pre = -1;
    while (Available()) {
        int b = Read();
        if (b < 0 || !bit_clazz_[b]) {
            Retract();
            break;
        }
        pre = b;
    }
    return pre;
}



inline void AbstractInput::PrepareBitClazz(const std::initializer_list<int>& list) {
    memset(bit_clazz_, false, 128);
    for (const auto &ch : list) {
        if (ch >= 0 && ch < 128) {
            bit_clazz_[ch] = true;
        }
    }
}

}
