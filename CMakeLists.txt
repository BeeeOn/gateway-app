# Top level build configuration
# TN, 2015

cmake_minimum_required (VERSION 2.8)

project (adaapp)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)
include (CheckCXXFunctionExists)
include (CheckCXXCompilerFlag)

# Default option definitions
option (POCO_NO_FLOAT "Force soft float in POCO Libraries" OFF)
option (ADAAPP_NO_SYSTEMD "Systemd not available on target system" OFF)

CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if (COMPILER_SUPPORTS_CXX11)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif (COMPILER_SUPPORTS_CXX0X)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else ()
        message (SEND_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif ()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -pedantic -Wextra")
if (NOT DEFINED ENV{PC_PLATFORM})
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
endif()

# check if we want soft float in POCO
if (POCO_NO_FLOAT)
    message (STATUS "Compiling without full floating point environment support in POCO")
    add_definitions (-DPOCO_NO_FPENVIRONMENT)
else ()
    message (STATUS "Compiling with full floating point environment support in POCO")
endif ()

# checking if the compiling is for PC or Olimex and add it to CXX_FLAGS
if (DEFINED ENV{PC_PLATFORM})
    message (STATUS "Compiler was set to PC platform.")
    add_definitions (-DPC_PLATFORM)
else ()
    message (STATUS "Compiler was set to OLinuXino platform.")
endif ()

# checking if we want to compile AdaApp with relative paths (for debugging) - add the relative prefix to compile script (e.g. ./compile_ant....sh .)
if (DEFINED ENV{RELATIVE_PATH})
    message (STATUS "Compiling AdaApp with RELATIVE PATH = '$ENV{RELATIVE_PATH}'.")
    add_definitions (-DRELATIVE_PATH=\"$ENV{RELATIVE_PATH}\")
endif ()

# checking FW version from git_tag and add it to CXX_FLAGS
if (DEFINED ENV{FW_VERSION})
    message (STATUS "FW_VERSION: $ENV{FW_VERSION}")
    add_definitions (-DFW_VERSION=\"$ENV{FW_VERSION}\")
else ()
    message (AUTHOR_WARNING "FW_VERSION WAS NOT DEFINED! Check 'git tag -l' if there is any tag with firmware version (tag syntax: \"demo_[0-9]+_\$FW_VERSION!\").")
endif ()

# the following functions are not defined in some versions of uClibc - define compatibility macros if that is the case
check_cxx_function_exists(std::to_string ${CXX_FUNCTION_FAMILY_TO_STRING} HAVE_CXX_TO_STRING)
check_cxx_function_exists(std::stoi ${CXX_FUNCTION_FAMILY_TO_NUMBER} HAVE_CXX_STOI)
check_cxx_function_exists(std::stoul ${CXX_FUNCTION_FAMILY_TO_NUMBER} HAVE_CXX_STOUL)
check_cxx_function_exists(std::stoull ${CXX_FUNCTION_FAMILY_TO_NUMBER} HAVE_CXX_STOULL)
check_cxx_function_exists(std::stof ${CXX_FUNCTION_FAMILY_TO_NUMBER} HAVE_CXX_STOF)

set (ADAAPP_SOURCES
	Distributor.cpp
	IOcontrol.cpp
	JSON.cpp HTTP.cpp
	ModuleADT.cpp
	MosqClient.cpp
	Openhab.cpp VPT.cpp
	PanInterface.cpp
	Parameters.cpp
	PressureSensor.cpp
	SerialControl.cpp
	TCP.cpp Aggregator.cpp
	VirtualSensor.cpp
	VirtualSensorModule.cpp
	VirtualSensorValue.cpp
	XMLTool.cpp
	main.cpp
)

add_executable (${PROJECT_NAME} ${ADAAPP_SOURCES})

find_package (Threads)
find_library (POCO_FOUNDATION PocoFoundation)
find_library (POCO_UTIL PocoUtil)
find_library (POCO_NET PocoNet)
find_library (POCO_XML PocoXML)
find_library (POCO_JSON PocoJSON)
find_library (POCO_NETSSL PocoNetSSL)
find_library (POCO_CRYPTO PocoCrypto)
find_library (MOSQUITTO_CPP mosquittopp)

target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${POCO_FOUNDATION} ${POCO_UTIL} ${POCO_NET} ${POCO_XML} ${POCO_NETSSL} ${POCO_CRYPTO} ${POCO_JSON} ${MOSQUITTO_CPP})

install (
	TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION usr/bin
)

install (
	DIRECTORY scripts/common/
	DESTINATION usr/bin
	FILE_PERMISSIONS
			OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
)

if (NOT ADAAPP_NO_SYSTEMD)

  install (
	DIRECTORY scripts/systemd/
	DESTINATION usr/bin
	FILE_PERMISSIONS
			OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
  )

endif ()

install (
	DIRECTORY etc/
	DESTINATION etc/beeeon
)

# creating new /var/lib/beeeon/ directory (if it doesn't exist already)
install (
	DIRECTORY DESTINATION var/lib/beeeon/
)

install (
	DIRECTORY DESTINATION tmp/
)
