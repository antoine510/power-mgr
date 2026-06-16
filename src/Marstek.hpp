#pragma once

#include <string>
#include <stdexcept>
#include "json/json_fwd.hpp"
#include <netinet/in.h>

class VenusE {
public:
	struct UsefulInfo {
		unsigned soc;
		int gridPower;
		unsigned gridInputEnergy;
		unsigned gridOutputEnergy;
	};

	class TimeoutError : public std::runtime_error {
	public:
		TimeoutError() : std::runtime_error("Timed out") {}
	};

	class ParseError : public std::runtime_error {
	public:
		ParseError() : std::runtime_error("Parse error") {}
	};

	VenusE(const std::string& ip, uint16_t port);
	~VenusE();

	nlohmann::json GetDevice(const std::string& bleMAC = "0");
	nlohmann::json GetWifiStatus(int id = 0);
	nlohmann::json GetBluetoothStatus(int id = 0);
	nlohmann::json GetBatteryStatus(int id = 0);
	nlohmann::json GetESStatus(int id = 0);
	nlohmann::json GetESMode(int id = 0);
	nlohmann::json GetEMStatus(int id = 0);

	UsefulInfo GetUsefulInfo(int id = 0);
private:
	nlohmann::json SendRequest(nlohmann::json&& req);
	int callID = 0;

	int _fd = -1;
	sockaddr_in _serverAddr{};
	char _recvBuffer[1024];
};
