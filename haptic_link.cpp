#include "haptic_link.h"

#include <chrono>
#include <iostream>
#include <thread>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

char commandToByte(Command cmd) {
    switch (cmd) {
        case Command::LEFT:  return '1';
        case Command::RIGHT: return '2';
        case Command::STOP:  return '3';
        case Command::GO:    return '4';
        default:             return 0;
    }
}

const char* commandName(Command cmd) {
    switch (cmd) {
        case Command::LEFT:  return "LEFT";
        case Command::RIGHT: return "RIGHT";
        case Command::STOP:  return "STOP";
        case Command::GO:    return "START(go)";
        default:             return "INVALID";
    }
}

bool HapticLink::open(const std::string& port) {
    close();
#ifdef _WIN32
    std::string path = port;
    if (path.rfind("\\\\.\\", 0) != 0) path = "\\\\.\\" + path;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        std::cerr << "[haptic] could not open " << port << "\n";
        return false;
    }
    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) { CloseHandle(h); return false; }
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(h, &dcb)) { CloseHandle(h); return false; }
    COMMTIMEOUTS to = {};
    to.WriteTotalTimeoutConstant = 200;
    SetCommTimeouts(h, &to);
    handle_ = reinterpret_cast<intptr_t>(h);
#else
    int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) {
        std::cerr << "[haptic] could not open " << port << "\n";
        return false;
    }
    termios tty = {};
    if (tcgetattr(fd, &tty) != 0) { ::close(fd); return false; }
    cfmakeraw(&tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(CSTOPB | PARENB);
    if (tcsetattr(fd, TCSANOW, &tty) != 0) { ::close(fd); return false; }
    handle_ = fd;
#endif
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "[haptic] connected to " << port << "\n";
    return true;
}

void HapticLink::close() {
    if (handle_ == kInvalid) return;
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
    ::close(static_cast<int>(handle_));
#endif
    handle_ = kInvalid;
}

bool HapticLink::send(Command cmd) {
    const char byte = commandToByte(cmd);
    if (byte == 0) return false; //nothing to send

    const auto now = std::chrono::steady_clock::now();
    if (has_last_ && cmd != Command::STOP) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_sent_).count();
        if (ms < static_cast<long long>(cooldown_ms_)) {
            std::cout << "[haptic] " << commandName(cmd) << " suppressed (cooldown)\n";
            return false;
        }
    }

    if (!isOpen()) {
        std::cout << "[haptic] (dry run) " << commandName(cmd) << " -> '" << byte << "'\n";
    } else {
        bool ok;
#ifdef _WIN32
        DWORD written = 0;
        ok = WriteFile(reinterpret_cast<HANDLE>(handle_), &byte, 1, &written, nullptr) && written == 1;
#else
        ok = ::write(static_cast<int>(handle_), &byte, 1) == 1;
#endif
        if (!ok) { std::cerr << "[haptic] write failed\n"; return false; }
        std::cout << "[haptic] sent " << commandName(cmd) << " -> '" << byte << "'\n";
    }
    has_last_ = true;
    last_sent_ = now;
    return true;
}