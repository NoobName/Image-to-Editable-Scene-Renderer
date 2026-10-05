#pragma once
#include <windows.h>
#include <source_location>
#include <sstream>
#include <stdexcept>

namespace isr {
inline void Check(HRESULT result, std::source_location where = std::source_location::current()) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << where.file_name() << ':' << where.line() << " HRESULT=0x"
                << std::hex << static_cast<unsigned long>(result);
        throw std::runtime_error(message.str());
    }
}
inline void CheckWin32(BOOL result) {
    if (!result) Check(HRESULT_FROM_WIN32(GetLastError()));
}
class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE value = nullptr) : value_(value) {}
    ~UniqueHandle() { if (value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    HANDLE Get() const { return value_; }
private:
    HANDLE value_;
};
}
