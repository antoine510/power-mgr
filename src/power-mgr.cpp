
#include <iostream>
#include <thread>
#include <signal.h>
#include "power-meter.hpp"
#include "power-meter-data.h"
#include "SolarHeater.hpp"
#include "IR/heat-pump.hpp"
#include "SHTC3.hpp"
#include "ws2811/ws2811.h"
#include <influxdb.hpp>

static constexpr const char* first_serial_device = "/dev/ttyUSB0";
static constexpr const char* second_serial_device = "/dev/ttyUSB1";
static constexpr const char* heater_serial_device = "/dev/serial0";
static constexpr const char* irled_serial_device = "/dev/lirc0";

static constexpr const char* influxdb_org_name = "PowerPi";
static constexpr const char* influxdb_house_bucket = "HousePower";

using LogPeriod = std::chrono::minutes;
using ExtraLogPeriod = std::chrono::hours;

static constexpr const std::chrono::seconds sample_interval = std::chrono::seconds(5);
static constexpr const auto sample_count = LogPeriod(1) / sample_interval;

std::unique_ptr<PowerMeter> houseMeter, solarMeter;
std::unique_ptr<SolarHeater> solarHeater;
std::unique_ptr<HeatPump> heatPump;
bool running = true;

PowerData houseData[sample_count], solarData[sample_count];
PowerData averageSamples(PowerData* dataArray) {
	PowerData res{};
	for(unsigned i = 0; i < sample_count; ++i) {
		res.current_ma += dataArray[i].current_ma;
		res.power_dw += dataArray[i].power_dw;
		res.voltage_dv += dataArray[i].voltage_dv;
		res.power_factor += dataArray[i].power_factor;
	}
	res.current_ma /= sample_count;
	res.power_dw /= sample_count;
	res.voltage_dv /= sample_count;
	res.power_factor /= sample_count;
	res.energy_wh = dataArray[sample_count - 1].energy_wh;	// Latest energy
	return res;
}

void startWaterHeaterHandler(int) {
	if(solarHeater) solarHeater->StartHeating();
}
void stopService(int) {
	running = false;
}

int main(int argc, char** argv) {
	SHTC3 shtc3;
	ws2811_t leds = {
		.freq = WS2811_TARGET_FREQ,
		.dmanum = 10,
		.channel = {
			[0] = {
				.gpionum = 18,
				.invert = 0,
				.count = 21,
				.strip_type = WS2811_STRIP_GRB,
				.brightness = 64,
			},
			[1] = {
				.gpionum = 0,
				.invert = 0,
				.count = 0,
				.brightness = 0,
			}
		}
	};
	auto led_status = ws2811_init(&leds);
	if(led_status != 0) return -1;

	signal(SIGINT, stopService);
	signal(SIGTERM, stopService);
	signal(SIGUSR1, startWaterHeaterHandler);

	std::string influxdb_token;
	try {
		auto env_token = getenv("INFLUXDB_TOKEN");
		if(!env_token) throw std::invalid_argument("Missing INFLUXDB_TOKEN environment variable");
		influxdb_token = env_token;

		auto firstMeter = std::make_unique<PowerMeter>(first_serial_device);
		auto secondMeter = std::make_unique<PowerMeter>(second_serial_device);
		PowerData firstData = firstMeter->ReadAll(), secondData = secondMeter->ReadAll();
		houseMeter = std::move(firstData.energy_wh < secondData.energy_wh ? firstMeter : secondMeter);
		solarMeter = std::move(firstData.energy_wh < secondData.energy_wh ? secondMeter : firstMeter);
	} catch(const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	influxdb_cpp::server_info serverInfo("127.0.0.1", 8086, influxdb_org_name, influxdb_token, influxdb_house_bucket);

	influxdb_cpp::builder()
		.meas("HouseExtra")
		.field("restart", true)
		.post_http(serverInfo);

	auto nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(std::chrono::system_clock::now());

	while(running) {
		const auto currentTP = std::chrono::system_clock::now();
		const auto nextMinuteTP = std::chrono::ceil<LogPeriod>(currentTP);
		std::this_thread::sleep_until(nextMinuteTP);	// Perform measurments at minute marks

		if(!solarHeater) {
			try {
				solarHeater = std::make_unique<SolarHeater>(heater_serial_device);
			} catch(const std::exception&) {}
		}

		/*if(!heatPump) {
			try {
				heatPump = std::make_unique<HeatPump>(irled_serial_device);
			} catch(const std::exception&) {}
		}*/

		try {
			for(int i = 0; i < sample_count; ++i) {
				houseData[i] = houseMeter->ReadAll();
				solarData[i] = solarMeter->ReadAll();

				int pdiff_dw = solarData[i].power_dw - houseData[i].power_dw;
				int led_count = abs(pdiff_dw) / 1000;
				int led_frac = (abs(pdiff_dw) - led_count * 1000) * 256 / 1000;
				if(led_count > 20) led_count = 20;
				uint32_t color = pdiff_dw > 0 ? 8 : 16;
				int li = 0;
				while(li < led_count) leds.channel[0].leds[20 - li++] = 0xff << color;
				leds.channel[0].leds[20 - li++] = led_frac << color;
				while(li < 21) leds.channel[0].leds[20 - li++] = 0;
				ws2811_render(&leds);

				if(heatPump) heatPump->PowerUpdate(houseData[i].power_dw, solarData[i].power_dw);

				if(i < sample_count - 1) std::this_thread::sleep_for(sample_interval - std::chrono::milliseconds(150));
			}

			PowerData houseAverage = averageSamples(houseData), solarAverage = averageSamples(solarData);

			shtc3.Measure();

			influxdb_cpp::builder()
				.meas("House")
				.field("voltage", houseAverage.voltage_dv / 10.f, 1)
				.field("current", houseAverage.current_ma / 1000.f, 3)
				.field("power", houseAverage.power_dw / 10.f, 1)
				.field("cos_phi", houseAverage.power_factor / 100.f, 2)
				.field("current_solar", solarAverage.current_ma / 1000.f, 3)
				.field("power_solar", solarAverage.power_dw / 10.f, 1)
				.field("temperature", shtc3.GetTemp_mC() / 1000.f, 2)
				.field("humidity", shtc3.GetRH_permille() / 10.f, 1)
				.post_http(serverInfo);

			if(currentTP > nextExtraTP) {
				influxdb_cpp::builder()
					.meas("HouseExtra")
					.field("energy", houseAverage.energy_wh / 1000.f, 3)
					.field("energy_solar", solarAverage.energy_wh / 1000.f, 3)
					.post_http(serverInfo);
				nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(currentTP);
			}

			if(solarHeater) {
				HeaterData heaterData = solarHeater->ReadData();

				influxdb_cpp::builder()
					.meas("Heater")
					.field("temperature", heaterData.temp_dC / 10.f, 1)
					.field("heater_on", heaterData.heater_on)
					.post_http(serverInfo);
			}

			if(heatPump) {
				int powerLevel = heatPump->GetCurrentPower();

				influxdb_cpp::builder()
					.meas("HeatPump")
					.field("power_level", powerLevel)
					.post_http(serverInfo);
			}
		} catch(const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}
	}
	ws2811_fini(&leds);

	return 0;
}
