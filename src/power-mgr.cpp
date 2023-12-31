#include <iostream>
#include <thread>
#include "power-meter.hpp"
#include "power-meter-data.h"
#include "SolarHeater.hpp"
#include <influxdb.hpp>

static constexpr const char* first_serial_device = "/dev/ttyUSB0";
static constexpr const char* second_serial_device = "/dev/ttyUSB1";
static constexpr const char* heater_serial_device = "/dev/serial0";

static constexpr const char* influxdb_org_name = "PowerPi";
static constexpr const char* influxdb_house_bucket = "HousePower";

using LogPeriod = std::chrono::minutes;
using ExtraLogPeriod = std::chrono::hours;

static constexpr const std::chrono::seconds sample_interval = std::chrono::seconds(5);
static constexpr const auto sample_count = LogPeriod(1) / sample_interval;


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

int main(int argc, char** argv) {
	std::unique_ptr<PowerMeter> houseMeter, solarMeter;
	std::unique_ptr<SolarHeater> solarHeater;

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

		solarHeater = std::make_unique<SolarHeater>(heater_serial_device);
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

	while(true) {
		const auto currentTP = std::chrono::system_clock::now();
		const auto nextMinuteTP = std::chrono::ceil<LogPeriod>(currentTP);
		std::this_thread::sleep_until(nextMinuteTP);	// Perform measurments at minute marks

		try {
			for(int i = 0; i < sample_count; ++i) {
				houseData[i] = houseMeter->ReadAll();
				solarData[i] = solarMeter->ReadAll();

				if(solarData[i].power_dw < 100) {	// If production is bellow 10W, it is actually consumption
					houseData[i].power_dw += solarData[i].power_dw;
					solarData[i].power_dw = 0;
				}

				if(i < sample_count - 1) std::this_thread::sleep_for(sample_interval - std::chrono::milliseconds(150));
			}

			PowerData houseAverage = averageSamples(houseData), solarAverage = averageSamples(solarData);
			HeaterData heaterData = solarHeater->ReadAll();

			influxdb_cpp::builder()
				.meas("House")
				.field("voltage", houseAverage.voltage_dv / 10.f, 1)
				.field("current", houseAverage.current_ma / 1000.f, 3)
				.field("power", houseAverage.power_dw / 10.f, 1)
				.field("cos_phi", houseAverage.power_factor / 100.f, 2)
				.field("current_solar", solarAverage.current_ma / 1000.f, 3)
				.field("power_solar", solarAverage.power_dw / 10.f, 1)
				.field("cos_phi_solar", solarAverage.power_factor / 100.f, 2)
				.post_http(serverInfo);

			if(currentTP > nextExtraTP) {
				influxdb_cpp::builder()
					.meas("HouseExtra")
					.field("energy", houseAverage.energy_wh / 1000.f, 3)
					.field("energy_solar", solarAverage.energy_wh / 1000.f, 3)
					.post_http(serverInfo);
				nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(currentTP);
			}

			influxdb_cpp::builder()
				.meas("Heater")
				.field("temperature", heaterData.temp_dC / 10.f, 1)
				.post_http(serverInfo);
		} catch(const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}
	}

	return 0;
}
