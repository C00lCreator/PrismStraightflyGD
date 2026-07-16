cmake_minimum_required(VERSION 3.21)

project(PrismStraightFlyHelper VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{GEODE_SDK} OR NOT EXISTS "$ENV{GEODE_SDK}/CMakeLists.txt")
    message(FATAL_ERROR "GEODE_SDK is missing or invalid. Install the Geode SDK with the Geode CLI first.")
endif()

add_library(${PROJECT_NAME} SHARED src/main.cpp)
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})