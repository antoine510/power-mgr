#include "SHTC3.hpp"
#include <thread>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <byteswap.h>

extern "C" {
	#include <linux/i2c.h>
	#include <linux/i2c-dev.h>
}

constexpr const char devName[] = "/dev/i2c-1";

constexpr uint8_t i2cAddress = 0x70;
constexpr uint16_t resetCmd = 0x5d80;
constexpr uint16_t sleepCmd = 0x98b0;
constexpr uint16_t wakeCmd = 0x1735;
constexpr uint16_t measureCmd = 0xa27c;


SHTC3::SHTC3() {
	_fd = ::open(devName, O_RDWR);
	if(_fd < 0) throw std::runtime_error("Error opening " + std::string(devName) + ": " + std::to_string(_fd));

	if(int res = ioctl(_fd, I2C_SLAVE, i2cAddress); res < 0)
		throw std::runtime_error("I2C cannot set address : " + std::to_string(res));

	::write(_fd, &wakeCmd, sizeof(wakeCmd));
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	::write(_fd, &resetCmd, sizeof(resetCmd));
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	::write(_fd, &sleepCmd, sizeof(sleepCmd));
}

SHTC3::~SHTC3() {
	::close(_fd);
}

#include <iostream>

void SHTC3::Measure() {
	static uint8_t buf[6];
	::write(_fd, &wakeCmd, sizeof(wakeCmd));
	std::this_thread::sleep_for(std::chrono::microseconds(250));
	::write(_fd, &measureCmd, sizeof(measureCmd));
	std::this_thread::sleep_for(std::chrono::milliseconds(13));
	::read(_fd, &_data, sizeof(_data));
	::write(_fd, &sleepCmd, sizeof(sleepCmd));
	_data.rawTemp = bswap_16(_data.rawTemp);
	_data.rawRH = bswap_16(_data.rawRH);
}
