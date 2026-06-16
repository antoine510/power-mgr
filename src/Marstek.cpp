#include "Marstek.hpp"
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include "json/json.hpp"

VenusE::VenusE(const std::string& ip, uint16_t port) {
	_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(_fd < 0) throw std::runtime_error("Couldn't create UDP socket");

	_serverAddr.sin_family = AF_INET;
	_serverAddr.sin_port = htons(port);
	_serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
}

VenusE::~VenusE() {
	if(_fd > 0) close(_fd);
}

nlohmann::json VenusE::GetDevice(const std::string& bleMAC) {
	return SendRequest({{"method", "Marstek.GetDevice"}, {"params", {{"ble_mac", bleMAC}}}});
}

nlohmann::json VenusE::GetWifiStatus(int id) {
	return SendRequest({{"method", "Wifi.GetStatus"}, {"params", {{"id", id}}}});
}

nlohmann::json VenusE::GetBluetoothStatus(int id) {
	return SendRequest({{"method", "BLE.GetStatus"}, {"params", {{"id", id}}}});
}

nlohmann::json VenusE::GetBatteryStatus(int id) {
	return SendRequest({{"method", "Bat.GetStatus"}, {"params", {{"id", id}}}});
}

nlohmann::json VenusE::GetESStatus(int id) {
	return SendRequest({{"method", "ES.GetStatus"}, {"params", {{"id", id}}}});
}

nlohmann::json VenusE::GetESMode(int id) {
	return SendRequest({{"method", "ES.GetMode"}, {"params", {{"id", id}}}});
}

nlohmann::json VenusE::GetEMStatus(int id) {
	return SendRequest({{"method", "EM.GetStatus"}, {"params", {{"id", id}}}});
}

VenusE::UsefulInfo VenusE::GetUsefulInfo(int id) {
	const auto json = GetESStatus(id);
	if(json.contains("error")) {
		int errorCode = json["error"]["code"].get<int>();
		if(errorCode == -32700) {	// Parse error
			throw ParseError();
		} else {
			throw std::runtime_error(json.dump());
		}
	}
	const auto& result = json.at("result");
	return UsefulInfo{result.at("bat_soc"), result.at("ongrid_power"), result.at("total_grid_input_energy"), result.at("total_grid_output_energy")};
}

nlohmann::json VenusE::SendRequest(nlohmann::json&& req) {
	req["id"] = callID++;
	std::string outBuf = req.dump();
	if(sendto(_fd, outBuf.data(), outBuf.size(), 0, (sockaddr*)&_serverAddr, sizeof(_serverAddr)) <= 0) throw std::runtime_error("Marstek send error: " + std::to_string(errno));
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
	int numRead = 0;
	while(true) {
		numRead = recv(_fd, _recvBuffer, sizeof(_recvBuffer), MSG_DONTWAIT);
		if(numRead > 0) break;
		if(errno == EAGAIN) {
			if(std::chrono::steady_clock::now() > deadline) throw TimeoutError();
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		} else {
			throw std::runtime_error("Marstek recv error: " + std::to_string(errno));
		}
	}
	return nlohmann::json::parse(std::string_view(_recvBuffer, numRead));
}
