#include "meteofrance.hpp"
#include <stdexcept>
#include <curl/curl.h>
#include "json/json.hpp"

#include <iostream>

MeteoFrance::MeteoFrance(std::string&& token, std::string&& stationID) : _token(std::move(token)), _stationID(std::move(stationID)) {
	if(auto code = curl_global_init(CURL_GLOBAL_ALL); code != CURLE_OK) throw std::runtime_error("libcurl init failure: " + std::to_string(code));
	if(_curl = curl_easy_init(); !_curl) throw std::runtime_error("Couldn't init curl");
	if(auto code = curl_easy_setopt(_curl, CURLOPT_URL, ("https://public-api.meteofrance.fr/public/DPObs/v2/station/infrahoraire-6m?id_station=" + _stationID + "&format=json").c_str()); code != CURLE_OK) throw std::runtime_error("setopt url failed: " + std::to_string(code));
	_headers = curl_slist_append((curl_slist*)_headers, "accept: */*");
	_headers = curl_slist_append((curl_slist*)_headers, ("apikey: " + _token).c_str());
	if(auto code = curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _headers); code != CURLE_OK) throw std::runtime_error("setopt header failed: " + std::to_string(code));
	if(auto code = curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, &MeteoFrance::onResponse); code != CURLE_OK) throw std::runtime_error("setopt writefun failed: " + std::to_string(code));
	if(auto code = curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this); code != CURLE_OK) throw std::runtime_error("setopt writedata failed: " + std::to_string(code));
	if(auto code = curl_easy_setopt(_curl, CURLOPT_TIMEOUT, 5); code != CURLE_OK) throw std::runtime_error("setopt timeout failed: " + std::to_string(code));
}

MeteoFrance::~MeteoFrance() {
	curl_slist_free_all((curl_slist*)_headers);
	curl_easy_cleanup(_curl);
	curl_global_cleanup();
}

MeteoFrance::WeatherData MeteoFrance::GetWeatherData() {
	if(auto res = curl_easy_perform(_curl); res != CURLE_OK) throw std::runtime_error("Cannot get weather data: " + std::to_string(res));
	long responseCode;
	curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &responseCode);
	if(responseCode != 200) throw std::runtime_error("HTTP error getting weather data: " + std::to_string(responseCode));
	auto json = nlohmann::json::parse(_response)[0];
	WeatherData res{};
	res.temp_c = json["t"].get<float>() - 273.15f;
	res.dewPoint_c = json["td"].get<float>() - 273.15f;
	res.humidity_rh = json["u"];
	res.windDir_deg = json["dd"];
	res.wind_mps = json["ff"];
	res.rafaleDir_deg= json["ddraf10"];
	res.rafale_mps = json["raf10"];
	res.rain_mm = json["rr_per"];
	res.visibility = json["vv"];
	res.irradiance_Jpm2 = json["ray_glo01"];
	res.press_Pa = json["pres"];
	res.pressMSL_Pa = json["pmer"];
	return res;
}

size_t MeteoFrance::onResponse(void* content, size_t size, size_t nmemb, void* userdata) {
	MeteoFrance* thiz = (MeteoFrance*)userdata;
	thiz->_response = std::string((char*)content, nmemb);
	return nmemb;
}
