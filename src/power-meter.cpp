#include "power-meter.hpp"

#define READ_U16 (uint16_t)((bytes[i++] << 8) | bytes[i++])
#define READ_U32 (uint32_t)(READ_U16 + (READ_U16 << 16))

PowerMeter::PowerMeter(const std::string& path) : _serial(path, 9600) {}

PowerMeter::Data PowerMeter::ReadAll() {
	auto bytes = _serial.SendCommandResponse(Cmd_ReadAll, sizeof(Cmd_ReadAll), 21);
	Data res;
	int i = 2;
	if(bytes[i++] != 0x12) throw std::runtime_error("Power-meter communication error!");
	res.voltage_dv = READ_U16;
	res.current_ma = READ_U32;
	res.power_dw = READ_U32;
	res.energy_wh = READ_U32;
	res.frequency_dhz = READ_U16;
	res.power_factor = READ_U16;
	return res;
}

PowerMeter::Data PowerMeter::GetAverageData() {
	if(!_samples.size()) throw std::runtime_error("No samples to average");
	PowerMeter::Data res{};
	for(const auto& sample : _samples) {
		res.voltage_dv += sample.voltage_dv;
		res.current_ma += sample.current_ma;
		res.power_dw += sample.power_dw;
		res.frequency_dhz += sample.frequency_dhz;
		res.power_factor += sample.power_factor;
	}
	res.voltage_dv /= _samples.size();
	res.current_ma /= _samples.size();
	res.power_dw /= _samples.size();
	res.frequency_dhz /= _samples.size();
	res.power_factor /= _samples.size();
	res.energy_wh = _samples.back().energy_wh;

	_samples.clear();
	return res;
}
