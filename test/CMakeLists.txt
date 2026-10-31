# Host-side unit tests for the hardware-independent code (protocols, drivers).
# Uses the Unity copy shipped with ESP-IDF; run inside the IDF container.
cmake_minimum_required(VERSION 3.16)
project(acaia-bridge-tests CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(UNITY_DIR $ENV{IDF_PATH}/components/unity/unity/src)
set(MAIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../main)

file(GLOB TEST_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
file(GLOB SCALE_DRIVER_SRCS CONFIGURE_DEPENDS ${MAIN_DIR}/scale/drivers/*.cpp)

add_executable(host_tests
    ${TEST_SRCS}
    ${SCALE_DRIVER_SRCS}
    ${MAIN_DIR}/acaia/AcaiaProtocol.cpp
    ${MAIN_DIR}/acaia/DeviceInfo.cpp
    ${MAIN_DIR}/scale/AdvertisingData.cpp
    ${MAIN_DIR}/scale/Driver.cpp
    ${MAIN_DIR}/status/LedPattern.cpp
    ${UNITY_DIR}/unity.c
)
target_include_directories(host_tests PRIVATE ${MAIN_DIR} ${UNITY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(host_tests PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
