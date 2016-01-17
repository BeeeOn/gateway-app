# Top level build configuration
# TN, 2015

cmake_minimum_required (VERSION 2.8)

project (adaapp)

include (CheckCXXCompilerFlag)
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

# checking if the compiling is for PC or Olimex and add it to CXX_FLAGS
if (DEFINED ENV{PC_PLATFORM})
    message (STATUS "Compiler was set to PC platform.")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPC_PLATFORM")
else ()
    message (STATUS "Compiler was set to OLinuXino platform.")
endif ()

# checking if we want to compile AdaApp with relative paths (for debugging) - add any parameter to compile script (e.g. ./compile_ant....sh RP)
if (DEFINED ENV{RELATIVE_PATHS})
    message (STATUS "Compiler was set to compile AdaApp with RELATIVE PATHS!.")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DRELATIVE_PATHS")
endif ()

# checking FW version from git_tag and add it to CXX_FLAGS
if (DEFINED ENV{FW_VERSION})
    message (STATUS "FW_VERSION: $ENV{FW_VERSION}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFW_VERSION='\"$ENV{FW_VERSION}\"'")
else ()
    message (AUTHOR_WARNING "FW_VERSION WAS NOT DEFINED! Check 'git tag -l' if there is any tag with firmware version (tag syntax: \"demo_[0-9]+_\$FW_VERSION!\").")
endif ()

set (ADAAPP_SOURCES main.cpp XMLTool.cpp PanInterface.cpp TCP.cpp Aggregator.cpp VirtualSensorModule.cpp VirtualSensor.cpp VirtualSensorValue.cpp Distributor.cpp IOcontrol.cpp MosqClient.cpp PressureSensor.cpp Openhab.cpp VPT.cpp JSON.cpp HTTP.cpp)

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
	DIRECTORY scripts/
	DESTINATION usr/bin
	FILE_PERMISSIONS
			OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
)

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
