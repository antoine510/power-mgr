
########
# Power manager
########

set(SOURCE_FILES
	Serial.hpp
	Serial.cpp

	SHTC3.hpp
	SHTC3.cpp

	ws281x.hpp
	ws281x.cpp

	MQTTComms.hpp
	MQTTComms.cpp

	power-meter.hpp
	power-meter.cpp

	power-mgr.cpp
)
add_executable(power-mgr ${SOURCE_FILES})

target_link_libraries(power-mgr PUBLIC yaml-cpp PahoMqttCpp::paho-mqttpp3 flatbuffers_target)

install(TARGETS power-mgr RUNTIME)
