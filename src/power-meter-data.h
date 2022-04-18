#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PowerData {
	uint32_t current_ma;
	uint32_t power_dw;
	uint32_t energy_wh;
	uint32_t voltage_dv;
	uint16_t frequency_dhz;
	uint16_t power_factor;
};

#ifdef __cplusplus
}
#endif
