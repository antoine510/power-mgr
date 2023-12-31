#include "SolarHeater.hpp"

#define READ_S16 (int16_t)(data[i++] | (data[i++] << 8))
#define READ_U16 (uint16_t)(data[i++] | (data[i++] << 8))
#define READ_U32 (uint32_t)(READ_U16 | (READ_U16 << 16))

SolarHeater::SolarHeater(const std::string& path) : _serial(path, 9600) {}

HeaterData SolarHeater::ReadAll() {
    static uint8_t readAllCmd[] = {0x4f, 0xc7, 0x01};
    _serial.Write(readAllCmd, sizeof(readAllCmd));
    HeaterData res;
	auto data = _serial.Read();
    int i = 0;
    res.temp_dC = READ_S16;
    return res;
}
