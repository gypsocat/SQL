#ifndef __MTB_EXCEPTION_H__
#define __MTB_EXCEPTION_H__

#include <exception>
#include <format>
#include <source_location>
#include <string>
#include <string_view>
namespace MTB {
    enum class ErrorLevel {
        NORMAL = 0,
        INFO, DEBUG, WARNING, CRITICAL, FATAL
    }; // enum class ErrorLevel

    class Exception: public std::exception {
    public:
        Exception(ErrorLevel level,
                  std::string_view msg,
                  std::source_location location = std::source_location::current()) noexcept
            : level(level), msg(msg) {
        }

        const char *what() const noexcept override { return msg.c_str(); }
    public:
        std::source_location location;
        ErrorLevel  level;
        std::string msg;
    }; // class Exception

    class NullException: public Exception {
    public:
        NullException(std::string_view pointer_name)
            : Exception(ErrorLevel::CRITICAL,
                std::format("NullException occured in {}", pointer_name)),
              pointer_name(pointer_name){}
    public:
        std::string pointer_name;
    }; // class NullException
} // namespace MTB

#endif
