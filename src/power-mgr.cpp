#include <iostream>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <signal.h>

#include "power-meter.hpp"
#include "SHTC3.hpp"
#include <influxdb.hpp>
#include <yaml-cpp/yaml.h>

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

	SHTC3 shtc3;

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
				auto powerAverage = powerMeters.at("Appart").GetAverageData();

				influxdb_cpp::builder builder;
				builder.meas("Power")
					.field("voltage", powerAverage.voltage_dv / 10.f, 1)
					.field("current", powerAverage.current_ma / 1000.f, 3)
					.field("power", powerAverage.power_dw / 10.f, 1)
					.field("cos_phi", powerAverage.power_factor / 100.f, 2)
					.field("temperature", shtc3.GetTemp_mC() / 1000.f, 2)
					.field("humidity", shtc3.GetRH_permille() / 10.f, 1);

				if(now > nextExtraTP) {
					((influxdb_cpp::detail::field_caller&)builder).field("energy", (long)powerAverage.energy_wh);
					nextExtraTP = std::chrono::ceil<ExtraLogPeriod>(now);
				}

				((influxdb_cpp::detail::field_caller&)builder).post_http(serverInfo);
			} catch(const std::exception& e) {
				std::cerr << e.what() << std::endl;
			}

			nextLogTP = std::chrono::ceil<LogPeriod>(now);
		}
	}

	return 0;
}
