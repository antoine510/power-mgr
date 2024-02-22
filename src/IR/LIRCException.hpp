#pragma once

#include <stdexcept>

class LIRCException : public std::runtime_error {
public:
    LIRCException(const char* what) : std::runtime_error(what) {}
    LIRCException(const char* device, const char* what) : std::runtime_error(std::string(device) + ": " + what) {}
};
