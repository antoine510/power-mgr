#pragma once

#include "Serial.hpp"

class WaterHeater {
public:
	WaterHeater(const std::string& path);

	struct __attribute__((packed)) Data {
		int16_t temp_dC;    // deciCelcius
		uint8_t heater_on;
	};

	Data ReadData() const;
	void StartHeating();
private:
	enum CommandID : uint8_t {
		NONE        = 0x0,
		READ_DATA   = 0x1,
		START_CYCLE = 0x2
	};
	static constexpr uint8_t MAGIC = 0x4f;

	Serial _serial;
};
