#include <iostream>
#include <thread>
#include "power-meter.hpp"
#include "power-meter-data.h"
#include <influxdb.hpp>

static constexpr const char* first_serial_device = "/dev/ttyUSB0";
static constexpr const char* second_serial_device = "/dev/ttyUSB1";

static constexpr const char* influxdb_org_name = "PowerPi";
static constexpr const char* influxdb_house_bucket = "HousePower";

int main(int argc, char** argv) {
	try {
		auto influxdb_token = getenv("INFLUXDB_TOKEN");
		if(!influxdb_token) throw std::invalid_argument("Missing INFLUXDB_TOKEN environment variable");

		PowerMeter firstMeter(first_serial_device), secondMeter(second_serial_device);
		influxdb_cpp::server_info serverInfo("127.0.0.1", 8086, influxdb_org_name, influxdb_token, influxdb_house_bucket);

		while(true) {
			const auto currentTP = std::chrono::system_clock::now();
			const auto nextMinuteTP = std::chrono::ceil<std::chrono::minutes>(currentTP);
			std::this_thread::sleep_until(nextMinuteTP);	// Perform measurments at minute marks

			try {
				PowerData firstData = firstMeter.ReadAll();
				PowerData secondData = secondMeter.ReadAll();
				PowerData& houseData = firstData.energy_wh > secondData.energy_wh ? firstData : secondData;
				PowerData& solarData = firstData.energy_wh > secondData.energy_wh ? secondData : firstData;

				influxdb_cpp::builder()
					.meas("House")
					.field("voltage", houseData.voltage_dv / 10.f)
					.field("current", houseData.current_ma / 1000.f)
					.field("power", houseData.power_dw / 10.f)
					.field("cos_phi", houseData.power_factor / 100.f)
					.field("current_solar", solarData.current_ma / 1000.f)
					.field("power_solar", solarData.power_dw / 10.f)
					.field("cos_phi_solar", solarData.power_factor / 100.f)
					.post_http(serverInfo);
			} catch(const std::exception& e) {
				std::cerr << e.what() << std::endl;
			}
		}
	} catch(const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}
