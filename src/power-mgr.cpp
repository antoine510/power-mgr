#include <iostream>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <memory>
#include <signal.h>

#include "power-meter.hpp"
#include <influxdb.hpp>
#include "ws281x.hpp"
#include <yaml-cpp/yaml.h>
#include "SHTC3.hpp"
#include "WaterHeater.hpp"
#include "Marstek.hpp"

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
std::unique_ptr<WaterHeater> waterHeater;
void startWaterHeaterHandler(int) {
	if(waterHeater) waterHeater->StartHeating();
}

int main(int argc, char** argv) {
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGUSR1, startWaterHeaterHandler);

	YAML::Node config = YAML::LoadFile("config.yaml");
	auto influxConfig = config["influx"];

	SHTC3 shtc3(config["shtc3"]["dev"].as<std::string>());

	ws281x::TSPIDriver spiDev(config["leds"]["dev"].as<std::string>(), ws281x::HZ_SPI_NEOPIXEL);
	const int ledCount = config["leds"]["count"].as<int>();
	const int brightness = config["leds"]["brightness"].as<int>();
	ws281x::TWS2812B leds[ledCount];

	std::unordered_map<std::string, PowerMeter> powerMeters;
	try {
		for(const std::pair<YAML::Node, YAML::Node>& pair : config["power-meters"]) {
			powerMeters.try_emplace(pair.first.as<std::string>(), pair.second["dev"].as<std::string>());
		}
	} catch(const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	VenusE battery(config["battery"]["ip"].as<std::string>(), config["battery"]["port"].as<unsigned>());
	VenusE::UsefulInfo latestBatteryInfo{};

	influxdb_cpp::server_info serverInfo(influxConfig["ip"].as<std::string>(), 8086, influxConfig["org"].as<std::string>(), influxConfig["token"].as<std::string>(), influxConfig["bucket"].as<std::string>());

	auto now = std::chrono::system_clock::now();
	auto nextLogTP = std::chrono::ceil<LogPeriod>(now);
	auto nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(now);

	std::unique_lock lk(serviceMutex);
	while(serviceRunning) {
		now = std::chrono::system_clock::now();
		
		if(serviceCV.wait_until(lk, std::chrono::ceil<SamplePeriod>(now)) != std::cv_status::timeout) continue;

		try {
			latestBatteryInfo = battery.GetUsefulInfo();
		} catch(std::runtime_error&) {}

		try {
			for(auto& pair : powerMeters) pair.second.TakeSample();

			int pdif_dw = (int)powerMeters.at("Solar").GetLatestData().power_dw - powerMeters.at("House").GetLatestData().power_dw;
			float litLedsFrac = std::abs(pdif_dw) / 1000.f;
			for(int i = 0; i < ledCount; ++i) {
				float ratio = std::max(std::min(litLedsFrac - i, 1.f), 0.f);
				if(pdif_dw < 0) leds[ledCount - i - 1].RGB(ratio * brightness, 0, 0);
				else leds[ledCount - i - 1].RGB(0, ratio * brightness, 0);
			}
			spiDev.SendData(leds, sizeof(leds));
		} catch(const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}
		if(!waterHeater) {
			try {
				waterHeater = std::make_unique<WaterHeater>(config["water-heater"]["dev"].as<std::string>());
			} catch(const std::exception& e) {
				std::cerr << e.what() << std::endl;
			}
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
					.field("current_solar", solarAverage.current_ma / 1000.f, 3)
					.field("power_solar", solarAverage.power_dw / 10.f, 1)
					.field("temperature", shtc3.GetTemp_mC() / 1000.f, 2)
					.field("humidity", shtc3.GetRH_permille() / 10.f, 1)
					.field("bat_soc", (int)latestBatteryInfo.soc)
					.field("bat_power", latestBatteryInfo.gridPower)
					.post_http(serverInfo);
				
				if(waterHeater) {
					auto heaterData = waterHeater->ReadData();

					influxdb_cpp::builder()
						.meas("Heater")
						.field("temperature", heaterData.temp_dC / 10.f, 1)
						.field("heater_on", (bool)heaterData.heater_on)
						.post_http(serverInfo);
				}

				if(now > nextExtraTP) {
					influxdb_cpp::builder()
						.meas("HouseExtra")
						.field("energy", houseAverage.energy_wh / 1000.f, 3)
						.field("energy_solar", solarAverage.energy_wh / 1000.f, 3)
						.field("battery_in", latestBatteryInfo.gridInputEnergy / 1000.f, 3)
						.field("battery_out", latestBatteryInfo.gridOutputEnergy/ 1000.f, 3)
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
