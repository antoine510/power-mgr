#pragma once

#include "SerialHandler.hpp"

struct HeaterData {
    int16_t temp_dC;    // deciCelcius
    bool heater_on;
};

class SolarHeater {
public:
	SolarHeater(const std::string& path);

	HeaterData ReadAll();
private:

	SerialHandler _serial;
};
