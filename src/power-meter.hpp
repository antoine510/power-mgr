#pragma once

#include "Serial.hpp"

class PowerMeter {
public:
	PowerMeter(const std::string& path);

	struct Data {
		uint32_t current_ma;
		uint32_t power_dw;
		uint32_t energy_wh;
		uint32_t voltage_dv;
		uint16_t frequency_dhz;
		uint16_t power_factor;
	};

	Data ReadAll();

	void TakeSample() { _samples.emplace_back(ReadAll()); }
	Data GetAverageData();
private:
	enum CmdWord : uint8_t {
		READ_SLAVE = 0x3,
		READ_MEASURE = 0x4,
		WRITE_SLAVE = 0x6
	};

	static constexpr const uint8_t Cmd_ReadAll[] = {0x01, 0x04, 0x0, 0x0, 0x0, 0x9, 0x30, 0x0c};
	static constexpr const uint8_t Cmd_ResetEnergy[] = {0x01, 0x42, 0x80, 0x11};
	static constexpr const uint8_t Cmd_Calibrate[] = {0xf8, 0x41, 0x37, 0x21, 0xb7, 0x78};

	Serial _serial;

	std::vector<Data> _samples;
};
