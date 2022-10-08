#include <iostream>
#include <thread>
#include "power-meter.hpp"
#include "power-meter-data.h"
#include <influxdb.hpp>

static constexpr const char* house_device = "/dev/power-house";
static constexpr const char* edf_solar_device = "/dev/power-solar";

static constexpr const char* influxdb_org_name = "PowerPi";
static constexpr const char* influxdb_house_bucket = "HousePower";

using LogPeriod = std::chrono::minutes;
using ExtraLogPeriod = std::chrono::hours;

static constexpr const std::chrono::seconds sample_interval = std::chrono::seconds(5);
static constexpr const auto sample_count = LogPeriod(1) / sample_interval;


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
	std::unique_ptr<PowerMeter> houseMeter, edfSolarMeter;
	std::string influxdb_token;
	try {
		auto env_token = getenv("INFLUXDB_TOKEN");
		if(!env_token) throw std::invalid_argument("Missing INFLUXDB_TOKEN environment variable");
		influxdb_token = env_token;

		houseMeter = std::make_unique<PowerMeter>(house_device);
		edfSolarMeter = std::make_unique<PowerMeter>(edf_solar_device);
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

				if(i < sample_count - 1) std::this_thread::sleep_for(sample_interval - std::chrono::milliseconds(150));
			}

			PowerData houseAverage = averageSamples(houseData);
			PowerData edfSolarData = edfSolarMeter->ReadAll();

			influxdb_cpp::builder()
				.meas("House")
				.field("voltage", houseAverage.voltage_dv / 10.f, 1)
				.field("current", houseAverage.current_ma / 1000.f, 3)
				.field("power", houseAverage.power_dw / 10.f, 1)
				.field("cos_phi", houseAverage.power_factor / 100.f, 2)
				.post_http(serverInfo);

			influxdb_cpp::builder()
				.meas("EDFSolar")
				.field("current", edfSolarData.current_ma / 1000.f, 3)
				.field("power", edfSolarData.power_dw / 10.f, 1)
				.field("cos_phi", edfSolarData.power_factor / 100.f, 2)
				.post_http(serverInfo);

			if(currentTP > nextExtraTP) {
				influxdb_cpp::builder()
					.meas("HouseExtra")
					.field("energy", houseAverage.energy_wh / 1000.f, 3)
					.field("edf_solar_energy", edfSolarData.energy_wh / 1000.f, 3)
					.post_http(serverInfo);
				nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(currentTP);
			}
		} catch(const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}
	}

	return 0;
}
