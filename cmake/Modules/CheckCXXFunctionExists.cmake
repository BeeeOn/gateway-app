# Check if a C++ function exists. Currently std::to_string and std::sto* are supported
# Adding support for a new family of functions can be accomplished by defining a new
# CXX_FUNCTION_FAMILY and adding the corresponding .cpp source file in the module directory
#
# The following variables may be set before calling this macro to
# modify the way the check is run:
#
#  CMAKE_REQUIRED_FLAGS = string of compile command line flags
#  CMAKE_REQUIRED_DEFINITIONS = list of macros to define (-DFOO=bar)
#  CMAKE_REQUIRED_INCLUDES = list of include directories
#  CMAKE_REQUIRED_LIBRARIES = list of libraries to link

#=============================================================================
# Copyright 2002-2011 Kitware, Inc.
# Copyright 2012 Bogdan Cristea
# Modified 2016 by Matej Postolka <xposto02@stud.fit.vutbr.cz>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

SET(CXX_FUNCTION_FAMILY_TO_STRING 0)
SET(CXX_FUNCTION_FAMILY_TO_NUMBER 1)

MACRO(CHECK_CXX_FUNCTION_EXISTS FUNCTION FAMILY VARIABLE)

  IF("${VARIABLE}" MATCHES "^${VARIABLE}$")
    
    SET(MACRO_CHECK_FUNCTION_DEFINITIONS 
      "-DCHECK_FUNCTION_EXISTS=${FUNCTION} ${CMAKE_REQUIRED_FLAGS}")
    
    MESSAGE(STATUS "Looking for ${FUNCTION}")

    IF(CMAKE_REQUIRED_LIBRARIES)
      SET(CHECK_FUNCTION_EXISTS_ADD_LIBRARIES
        "-DLINK_LIBRARIES:STRING=${CMAKE_REQUIRED_LIBRARIES}")
    ELSE()
      SET(CHECK_FUNCTION_EXISTS_ADD_LIBRARIES)
    ENDIF()

    IF(CMAKE_REQUIRED_INCLUDES)
      SET(CHECK_FUNCTION_EXISTS_ADD_INCLUDES
        "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_REQUIRED_INCLUDES}")
    ELSE()
      SET(CHECK_FUNCTION_EXISTS_ADD_INCLUDES)
    ENDIF()

    IF(${FAMILY} EQUAL ${CXX_FUNCTION_FAMILY_TO_STRING})
      SET(CXX_SOURCE_FILE ${CMAKE_MODULE_PATH}/CheckFunctionExistsToString.cpp)
    ELSEIF(${FAMILY} EQUAL ${CXX_FUNCTION_FAMILY_TO_NUMBER})
      SET(CXX_SOURCE_FILE ${CMAKE_MODULE_PATH}/CheckFunctionExistsToNumber.cpp)
    ELSE()
      MESSAGE(FATAL_ERROR "Unknown function family supplied in CHECK_CXX_FUNCTION_EXISTS")
    ENDIF()

    TRY_COMPILE(${VARIABLE}
      ${CMAKE_BINARY_DIR}
      ${CXX_SOURCE_FILE}
      COMPILE_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}"
      CMAKE_FLAGS "-DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_FUNCTION_DEFINITIONS} ${CHECK_FUNCTION_EXISTS_ADD_LIBRARIES} ${CHECK_FUNCTION_EXISTS_ADD_INCLUDES}"
      OUTPUT_VARIABLE OUTPUT)

    IF(${VARIABLE})
      SET(${VARIABLE} 1 CACHE INTERNAL "Have function ${FUNCTION}")
      ADD_DEFINITIONS(-D${VARIABLE})
      MESSAGE(STATUS "Looking for ${FUNCTION} - found")
      FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log 
        "Determining if the function ${FUNCTION} exists passed with the following output:\n"
        "${OUTPUT}\n\n")
    ELSE()
      MESSAGE(STATUS "Looking for ${FUNCTION} - not found")
      SET(${VARIABLE} "" CACHE INTERNAL "Have function ${FUNCTION}")
      FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log 
        "Determining if the function ${FUNCTION} exists failed with the following output:\n"
        "${OUTPUT}\n\n")
    ENDIF()

  ENDIF()

ENDMACRO(CHECK_CXX_FUNCTION_EXISTS)
