cmake_minimum_required(VERSION 3.13)

project(power-mgr)

message(STATUS "Running CMake for ${CMAKE_PROJECT_NAME}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/Install" CACHE PATH "Default install in main directory" FORCE)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(WINDOWS_HOST TRUE)
	set(WINDOWS_TARGET TRUE)

	add_definitions(-DWINDOWS -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
		set(ARM64_TARGET TRUE)
	endif()
	set(LINUX_HOST TRUE)
	set(LINUX_TARGET TRUE)
endif()

if(WINDOWS_TARGET)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(_ITERATOR_DEBUG_LEVEL 2)
	else()
		set(_ITERATOR_DEBUG_LEVEL 0)
	endif()
	add_definitions(-D_ITERATOR_DEBUG_LEVEL=${_ITERATOR_DEBUG_LEVEL})
elseif(LINUX_TARGET)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

include_directories("influxdb")

include_directories("src")
add_subdirectory("src")

