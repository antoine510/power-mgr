#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PowerData {
	uint16_t voltage_dv;
	uint32_t current_ma;
	uint32_t power_dw;
	uint32_t energy_wh;
	uint16_t frequency_dhz;
	uint16_t power_factor;
};

#ifdef __cplusplus
}
#endif
