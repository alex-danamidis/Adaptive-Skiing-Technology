#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include "commands.h"

//LEFT='1'  RIGHT='2'  STOP='3'  GO(start)='4'
char commandToByte(Command cmd);// 0 if cmd has no haptic pattern 
const char* commandName(Command cmd);

class HapticLink {
public:
    HapticLink() = default;
    ~HapticLink() { close(); }
    HapticLink(const HapticLink&) = delete;
    HapticLink& operator=(const HapticLink&) = delete;
    bool open(const std::string& port);
    void close();
    bool isOpen() const { return handle_ != kInvalid; }

    void setCooldownMs(unsigned ms) { cooldown_ms_ = ms; }
    bool send(Command cmd);

private:
    static constexpr intptr_t kInvalid = -1;
    intptr_t handle_ = kInvalid;
    unsigned cooldown_ms_ = 1500;
    bool has_last_ = false;
    std::chrono::steady_clock::time_point last_sent_;
};