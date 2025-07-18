#include "WaterHeater.hpp"

#define READ_S16 (int16_t)(data[i++] | (data[i++] << 8))
#define READ_U16 (uint16_t)(data[i++] | (data[i++] << 8))
#define READ_U32 (uint32_t)(READ_U16 | (READ_U16 << 16))

WaterHeater::WaterHeater(const std::string& path) : _serial(path, 9600) {}

WaterHeater::Data WaterHeater::ReadData() const {
	static uint8_t readAllCmd[] = {MAGIC, CommandID::READ_DATA};
	return *(Data*)(_serial.SendCommandResponse(readAllCmd, sizeof(readAllCmd), sizeof(Data)).data());
}

void WaterHeater::StartHeating() {
	static uint8_t cmd[] = {MAGIC, CommandID::START_CYCLE};
	_serial.SendCommand(cmd, sizeof(cmd));
}
