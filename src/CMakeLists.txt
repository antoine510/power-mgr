
set(SOURCE_FILES
	Serial.hpp
	Serial.cpp

	ws281x.hpp
	ws281x.cpp

	SHTC3.hpp
	SHTC3.cpp

	WaterHeater.hpp
	WaterHeater.cpp

	Marstek.hpp
	Marstek.cpp

	meteofrance.hpp
	meteofrance.cpp

	power-meter.hpp
	power-meter.cpp

	power-mgr.cpp
)
add_executable(power-mgr ${SOURCE_FILES})

target_link_libraries(power-mgr PUBLIC yaml-cpp CURL::libcurl)

install(TARGETS power-mgr RUNTIME)
