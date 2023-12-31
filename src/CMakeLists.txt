
########
# Power manager
########

set(SOURCE_FILES
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

if(WINDOWS_HOST)
	set_target_properties(power-mgr PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Install/bin/")
endif()

install(TARGETS power-mgr RUNTIME)
