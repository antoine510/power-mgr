#pragma once

#include <thread>

class HeatPump {
public:
	HeatPump(const std::string& path);
	~HeatPump();

	enum class Command {
		POWER_TOGGLE = 0xFF,
		TEMP_DOWN = 0xB0,
		INVERTER_POWER = 0xC8,
		TEMP_UP = 0xF0
	};
	void SendCommand(Command cmd);

	void PowerUpdate(unsigned consumed_dw, unsigned produced_dw);

	int GetCurrentPower() const { return _isOn ? _currentPower + 1 : 0; }

private:
	static constexpr const unsigned powerLow_dw = 4200, powerMid_dw = 6000, powerHigh_dw = 7000, powerUltra_dw = 9000;
	static constexpr const std::chrono::minutes turnOnHysteresis{10}, turnOffHysteresis{5}, powerHysteresis{1};

	void goToPower(int target);

	int _fd;

	bool _isOn = false;
	int _currentPower = 0;	// Low, Mid, High, Ultra, Auto
	bool _canTurnOn = false, _shallTurnOff = false;
	std::chrono::steady_clock::time_point _lastTurnOn, _lastTurnOff, _canTurnOnSince, _shallTurnOffSince, _lastPowerChange;

	std::thread _powerChangeThread;
};
