
########
# Power manager
########

set(SOURCE_FILES
	IR/LIRCException.hpp

	IR/ir-encode.cpp
	IR/ir-encode.hpp
	IR/ir.cpp
	IR/ir.hpp

	IR/heat-pump.hpp
	IR/heat-pump.cpp

	DS18B20.hpp
	DS18B20.cpp

	SerialHandler.hpp
	SerialHandler.cpp

	power-meter-data.h

	power-meter.hpp
	power-meter.cpp

	SolarHeater.cpp
	SolarHeater.hpp

	power-mgr.cpp
)
add_executable(power-mgr ${SOURCE_FILES})

target_link_libraries(power-mgr pthread)

install(TARGETS power-mgr RUNTIME)
