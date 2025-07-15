#include <iostream>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <signal.h>

#include "power-meter.hpp"
#include <influxdb.hpp>
#include <yaml-cpp/yaml.h>
#include "MQTTComms.hpp"
#include "SHTC3.hpp"
#include "ws281x.hpp"
#include "battery_generated.h"

using SamplePeriod = std::chrono::duration<int64_t, std::ratio<5>>;
using LogPeriod = std::chrono::minutes;
using ExtraLogPeriod = std::chrono::hours;	// Needs to be a multiple of LogPeriod

bool serviceRunning = true;
std::mutex serviceMutex;
std::condition_variable serviceCV;
void signalHandler(int signum) {
	std::cout << "Closing service" << std::endl;
	serviceRunning = false;
	serviceCV.notify_all();
}

int main(int argc, char** argv) {
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	YAML::Node config = YAML::LoadFile("config.yaml");
	auto influxConfig = config["influx"];

	SHTC3 shtc3(config["shtc3"]["dev"].as<std::string>());

	MQTTComms mqtt(influxConfig["ip"].as<std::string>(), "power-mgr");

	mqtt.Subscribe("Battery/House", 0, [&](mqtt::const_message_ptr msg) {
		constexpr int numLEDs = 20;
		constexpr int brightness = 64;

		static ws281x::TSPIDriver spiDev("/dev/spidev1.0", ws281x::HZ_SPI_NEOPIXEL);
		static ws281x::TWS2812B leds[numLEDs];
		const auto& bat = *flatbuffers::GetRoot<api::Battery>(msg->get_payload().data());
		float voltageRatio = (float)(bat.voltage() - 6800) / (8200 - 6800);

		for(int i = 0; i < numLEDs; ++i) {
			static float red[numLEDs] = {1, 1, 1, 1, 1, 1, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0, 0, 0, 0, 0};
			static float green[numLEDs] = {0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
			float ratio = std::max(std::min(voltageRatio * numLEDs - i, 1.f), 0.f);
			leds[i].RGB(ratio * red[i] * brightness, ratio * green[i] * brightness, 0);
		}
		spiDev.SendData(leds, sizeof(leds));
	});

	std::unordered_map<std::string, PowerMeter> powerMeters;
	try {
		for(const std::pair<YAML::Node, YAML::Node>& pair : config["power-meters"]) {
			powerMeters.try_emplace(pair.first.as<std::string>(), pair.second["dev"].as<std::string>());
		}
	} catch(const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	influxdb_cpp::server_info serverInfo(influxConfig["ip"].as<std::string>(), 8086, influxConfig["org"].as<std::string>(), influxConfig["token"].as<std::string>(), influxConfig["bucket"].as<std::string>());

	auto now = std::chrono::system_clock::now();
	auto nextLogTP = std::chrono::ceil<LogPeriod>(now);
	auto nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(now);

	std::unique_lock lk(serviceMutex);
	while(serviceRunning) {
		now = std::chrono::system_clock::now();

		if(serviceCV.wait_until(lk, std::chrono::ceil<SamplePeriod>(now)) != std::cv_status::timeout) continue;

		try {
			for(auto& pair : powerMeters) pair.second.TakeSample();
		} catch(const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}

		if(now > nextLogTP) {
			try {
				shtc3.Measure();
				auto houseAverage = powerMeters.at("House").GetAverageData();
				auto solarAverage = powerMeters.at("Solar").GetAverageData();

				influxdb_cpp::builder()
					.meas("House")
					.field("voltage", houseAverage.voltage_dv / 10.f, 1)
					.field("current", houseAverage.current_ma / 1000.f, 3)
					.field("power", houseAverage.power_dw / 10.f, 1)
					.field("cos_phi", houseAverage.power_factor / 100.f, 2)
					.field("temperature", shtc3.GetTemp_mC() / 1000.f, 2)
					.field("humidity", shtc3.GetRH_permille() / 10.f, 1)
					.post_http(serverInfo);

				influxdb_cpp::builder()
					.meas("Solar")
					.field("current", solarAverage.current_ma / 1000.f, 3)
					.field("power", solarAverage.power_dw / 10.f, 1)
					.post_http(serverInfo);

				if(now > nextExtraTP) {
					influxdb_cpp::builder()
						.meas("Extra")
						.field("house_e", houseAverage.energy_wh / 1000.f, 3)
						.field("solar_e", solarAverage.energy_wh / 1000.f, 3)
						.post_http(serverInfo);

					nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(now);
				}
			} catch(const std::exception& e) {
				std::cerr << e.what() << std::endl;
			}

			nextLogTP = std::chrono::ceil<LogPeriod>(now);
		}
	}

	return 0;
}
