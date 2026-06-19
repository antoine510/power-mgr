#pragma once

#include <string>

class MeteoFrance {
public:
	struct WeatherData {
		float temp_c;
		float dewPoint_c;
		int humidity_rh;
		int windDir_deg;
		float wind_mps;
		int rafaleDir_deg;
		float rafale_mps;
		float rain_mm;
		int visibility;
		int irradiance_Jpm2;
		int press_Pa;
		int pressMSL_Pa;
	};

	MeteoFrance(std::string&& token, std::string&& stationID);
	~MeteoFrance();

	WeatherData GetWeatherData();
private:
	static size_t onResponse(void *ptr, size_t size, size_t nmemb, void *stream);
	std::string _response;

	std::string _token, _stationID;
	void* _curl, *_headers = nullptr;
};
