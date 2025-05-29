#include <iostream>
#include <thread>
#include "power-meter.hpp"
#include "power-meter-data.h"
#include <influxdb.hpp>
#include "MQTTComms.hpp"
#include "battery_generated.h"
#include "ws281x.hpp"

static constexpr const char* house_device = "/dev/power-house";
static constexpr const char* solar_device = "/dev/power-solar";

static constexpr const char* influxdb_ip = "192.168.1.37";
static constexpr int influxdb_port = 8086;
static constexpr const char* influxdb_org_name = "Microtonome";
static constexpr const char* influxdb_bucket = "EDF";

static constexpr unsigned numLEDs = 20;

using LogPeriod = std::chrono::minutes;
using ExtraLogPeriod = std::chrono::hours;

static constexpr std::chrono::seconds sample_interval = std::chrono::seconds(5);
static constexpr auto sample_count = LogPeriod(1) / sample_interval;

static constexpr unsigned maxReadFails = 4;

PowerData houseData[sample_count];
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

int main(int argc, char** argv) {
	std::unique_ptr<PowerMeter> houseMeter, solarMeter;
	std::string influxdb_token;
	try {
		auto env_token = getenv("INFLUXDB_TOKEN");
		if(!env_token) throw std::invalid_argument("Missing INFLUXDB_TOKEN environment variable");
		influxdb_token = env_token;

		houseMeter = std::make_unique<PowerMeter>(house_device);
		solarMeter = std::make_unique<PowerMeter>(solar_device);
	} catch(const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	influxdb_cpp::server_info serverInfo(influxdb_ip, influxdb_port, influxdb_org_name, influxdb_token, influxdb_bucket);

	influxdb_cpp::builder()
		.meas("Extra")
		.field("restart", true)
		.post_http(serverInfo);

	ws281x::TSPIDriver spiDev("/dev/spidev1.0", ws281x::HZ_SPI_NEOPIXEL);
	ws281x::TWS2812B leds[numLEDs];

	MQTTComms mqtt("power-mgr");

	mqtt.Subscribe("Battery/House", 0, [&](mqtt::const_message_ptr msg) {
		const auto& bat = *flatbuffers::GetRoot<api::Battery>(msg->get_payload().data());
		float voltageRatio = (float)(bat.voltage() - 6800) / (8200 - 6800);
		constexpr int brightness = 64;

		for(int i = 0; i < 20; ++i) {
			leds[i].RGB(0, 0, std::max(std::min(voltageRatio * 20 - i, 1.f), 0.f) * brightness);
		}
		spiDev.SendData(leds, sizeof(leds));
	});

	auto nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(std::chrono::system_clock::now());
	unsigned readFailCount = 0;
	while(true) {
		const auto currentTP = std::chrono::system_clock::now();
		const auto nextMinuteTP = std::chrono::ceil<LogPeriod>(currentTP);
		std::this_thread::sleep_until(nextMinuteTP);	// Perform measurments at minute marks

		try {
			for(int i = 0; i < sample_count; ++i) {
				houseData[i] = houseMeter->ReadAll();

				if(i < sample_count - 1) std::this_thread::sleep_for(sample_interval - std::chrono::milliseconds(150));
			}

			PowerData houseAverage = averageSamples(houseData);
			PowerData solarData = solarMeter->ReadAll();

			influxdb_cpp::builder()
				.meas("House")
				.field("voltage", houseAverage.voltage_dv / 10.f, 1)
				.field("current", houseAverage.current_ma / 1000.f, 3)
				.field("power", houseAverage.power_dw / 10.f, 1)
				.field("cos_phi", houseAverage.power_factor / 100.f, 2)
				.post_http(serverInfo);

			influxdb_cpp::builder()
				.meas("Solar")
				.field("current", solarData.current_ma / 1000.f, 3)
				.field("power", solarData.power_dw / 10.f, 1)
				.field("cos_phi", solarData.power_factor / 100.f, 2)
				.post_http(serverInfo);

			if(currentTP > nextExtraTP) {
				influxdb_cpp::builder()
					.meas("Extra")
					.field("house_e", houseAverage.energy_wh / 1000.f, 3)
					.field("solar_e", solarData.energy_wh / 1000.f, 3)
					.post_http(serverInfo);
				nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(currentTP);
			}
			readFailCount = 0;
		} catch(const std::exception& e) {
			if(++readFailCount > maxReadFails) return -1;
			std::cerr << e.what() << std::endl;
		}
	}

	return 0;
}
