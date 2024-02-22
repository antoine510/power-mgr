#pragma once

class HeatPump {
public:
	HeatPump(const std::string& path);

	enum class Command {
		TEMP_DOWN = 0xB0,
		INVERTER_POWER = 0xC8,
		TEMP_UP = 0xF0
	};
	bool SendCommand(Command cmd);

private:
	int fd;
};
