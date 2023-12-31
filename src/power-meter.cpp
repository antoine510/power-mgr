#include "power-meter.hpp"
#include "power-meter-data.h"
#include <iostream>
#include <vector>
#include <thread>

PowerMeter::PowerMeter(const std::string& path) : _serial(path, 9600) {}

#define READ_U16 (uint16_t)((data[i++] << 8) | data[i++])
#define READ_U32 (uint32_t)(READ_U16 + (READ_U16 << 16))

PowerData PowerMeter::ReadAll() {
	_serial.Write(Cmd_ReadAll, sizeof(Cmd_ReadAll));
	PowerData res;
	auto data = _serial.Read();
	int i = 2;
	if(data[i++] != 0x12) throw std::runtime_error("Wrong response count!");
	res.voltage_dv = READ_U16;
	res.current_ma = READ_U32;
	res.power_dw = READ_U32;
	res.energy_wh = READ_U32;
	res.frequency_dhz = READ_U16;
	res.power_factor = READ_U16;
	return res;
}
