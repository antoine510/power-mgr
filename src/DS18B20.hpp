#pragma once

#include <string>

class DS18B20 {
public:
    DS18B20(std::string path) : _path(path) {}

    unsigned TakeMeasure() const;
private:
    std::string _path;
};
