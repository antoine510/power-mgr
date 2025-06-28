#pragma once

#include <stdint.h>

class SHTC3 {
public:
	SHTC3();
	~SHTC3();

	void Measure();

	int GetTemp_mC() const { return (int)_data.rawTemp * 21875 / 8192 - 45'000; }
	unsigned GetRH_permille() const { return (unsigned)_data.rawRH * 125 / 8192; }
protected:
	int _fd = 0;

	struct __attribute__((packed)) Data {
		uint16_t rawTemp;
		uint8_t tempCRC;
		uint16_t rawRH;
		uint8_t rhCRC;
	} _data{};
};

