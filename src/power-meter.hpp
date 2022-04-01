#pragma once

#include <string>
#include <vector>
#include <stdexcept>

#ifndef WINDOWS
#include <netinet/in.h>
#endif

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif


class PowerMeter {
public:
	PowerMeter(const std::string& path, int baudrate = 9600);
	~PowerMeter() noexcept;

	struct PowerData ReadAll();
private:
	enum CmdWord : uint8_t {
		READ_SLAVE = 0x3,
		READ_MEASURE = 0x4,
		WRITE_SLAVE = 0x6
	};

	static constexpr const uint8_t Cmd_ReadAll[] = {0xf8, 0x4, 0x0, 0x0, 0x0, 0x9, 0x65, 0x24};
	static constexpr const uint8_t Cmd_ResetEnergy[] = {0xf8, 0x42, 0x41, 0xc2};
	static constexpr const uint8_t Cmd_Calibrate[] = {0xf8, 0x41, 0x37, 0x21, 0x78, 0xb7};

	PACK(struct RWBuf {	// Big-Endian
		uint8_t slave_addr = 0xf8;
		CmdWord command;
		uint16_t address;
		uint16_t read_count_or_write_value;
		uint16_t cksum;
	});

	class ReadTimeoutException : public std::runtime_error {
	public:
		ReadTimeoutException() : std::runtime_error("Read timed-out") {}
	};

	uint16_t modbusCRC(uint8_t* data, int length) {
		uint16_t crc = 0xFFFF;
		for(int pos = 0; pos<length; pos++){
			crc ^= (uint16_t)data[pos];
			for( int i=0; i<8; i++ ){
				if(crc & 1){      // LSB is set
					crc >>= 1;                 // Shift right
					crc ^= 0xA001;             // XOR 0xA001
				}else{                         // LSB is not set
					crc >>= 1;
				};
			};
		};
		return crc;
	}

	template<typename T>
	uint8_t* finalizeBuf(T& buf) const {
		uint8_t* bytes = reinterpret_cast<uint8_t*>(&buf);
		buf.len = htons(sizeof(T) - sizeof(T::stx));
		buf.cksu
		buf.cksum = htonl(buf.cksum);
		return bytes;
	}

	std::vector<uint8_t> read() const;
	void write(const uint8_t* cmd_buf, size_t sz) const;

#ifndef WINDOWS
	int _fd = -1;
	struct timespec _timeout = { 0, 100000000 };	// 100 ms
#else
	void* _handle = nullptr;
#endif
};
