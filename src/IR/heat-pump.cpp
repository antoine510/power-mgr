#include "heat-pump.hpp"
#include "ir.hpp"
#include "LIRCException.hpp"
#include <thread>

using namespace IR;

HeatPump::HeatPump(const std::string& path) : _fd(lirc_open(path.c_str(), RC_PROTO_NEC)) {}
HeatPump::~HeatPump() {
	if(_powerChangeThread.joinable()) _powerChangeThread.join();
	lirc_close(_fd);
}

void HeatPump::SendCommand(Command cmd) {
	lirc_send(_fd, RC_PROTO_NEC, static_cast<unsigned>(cmd));
}

void HeatPump::goToPower(int target) {
	constexpr const int powerCount = 5;	// Low, Mid, High, Ultra, Auto

	if(target == _currentPower) return;

	auto now = std::chrono::steady_clock::now();
	if(now < _lastPowerChange + powerHysteresis) return;
	_lastPowerChange = now;

	if(_powerChangeThread.joinable()) _powerChangeThread.join();
	_powerChangeThread = std::thread([this](int presses) {
		for(int i = 0; i < presses; ++i) {
			SendCommand(Command::INVERTER_POWER);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}, (target - _currentPower + powerCount) % powerCount);
}

void HeatPump::PowerUpdate(unsigned consumed_dw, unsigned produced_dw) {
	unsigned available_dw = produced_dw - consumed_dw;
	if(_isOn) {
		if(available_dw > powerLow_dw) {
			if(available_dw > powerUltra_dw) {
				goToPower(3);
			} else if(available_dw > powerHigh_dw) {
				goToPower(2);
			} else if(available_dw > powerMid_dw) {
				goToPower(1);
			} else {
				goToPower(0);
			}
			_shallTurnOff = false;
		} else {
			auto now = std::chrono::steady_clock::now();
			if(!_shallTurnOff) {
				_shallTurnOff = true;
				_shallTurnOffSince = now;
			} else if(now > _shallTurnOffSince + turnOffHysteresis) {
				SendCommand(Command::POWER_TOGGLE);
				_isOn = false;
			}
		}
	} else {
		if(available_dw > powerLow_dw) {
			auto now = std::chrono::steady_clock::now();
			if(!_canTurnOn) {
				_canTurnOn = true;
				_canTurnOnSince = now;
			} else if(now > _canTurnOnSince + turnOnHysteresis) {
				SendCommand(Command::POWER_TOGGLE);
				_isOn = true;
			}
		} else {
			_canTurnOn = false;
		}
	}
}
