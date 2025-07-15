
########
# Power manager
########

set(SOURCE_FILES
	Serial.hpp
	Serial.cpp

	SHTC3.hpp
	SHTC3.cpp

	power-meter.hpp
	power-meter.cpp

	power-mgr.cpp
)
add_executable(power-mgr ${SOURCE_FILES})

target_link_libraries(power-mgr PUBLIC yaml-cpp)

install(TARGETS power-mgr RUNTIME)
