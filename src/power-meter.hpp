#pragma once

#include "SerialHandler.hpp"

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif


class PowerMeter {
public:
	PowerMeter(const std::string& path);

	struct PowerData ReadAll();
private:
	enum CmdWord : uint8_t {
		READ_SLAVE = 0x3,
		READ_MEASURE = 0x4,
		WRITE_SLAVE = 0x6
	};

	static constexpr const uint8_t Cmd_ReadAll[] = {0x01, 0x04, 0x0, 0x0, 0x0, 0x9, 0x30, 0x0c};
	static constexpr const uint8_t Cmd_ResetEnergy[] = {0x01, 0x42, 0x80, 0x11};
	static constexpr const uint8_t Cmd_Calibrate[] = {0xf8, 0x41, 0x37, 0x21, 0xb7, 0x78};

	PACK(struct RWBuf {	// Big-Endian
		uint8_t slave_addr = 0x01;
		CmdWord command;
		uint16_t address;
		uint16_t read_count_or_write_value;
		uint16_t cksum;
	});

	SerialHandler _serial;
};
