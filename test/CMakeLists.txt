# Host-side unit tests for the hardware-independent code (protocols, drivers).
# Uses the Unity copy shipped with ESP-IDF; run inside the IDF container.
cmake_minimum_required(VERSION 3.16)
project(acaia-bridge-tests CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(UNITY_DIR $ENV{IDF_PATH}/components/unity/unity/src)
set(MAIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../main)

add_executable(host_tests
    test_main.cpp
    test_acaia_protocol.cpp
    test_eureka_precisa.cpp
    test_driver_registry.cpp
    test_advertising_data.cpp
    test_led_pattern.cpp
    test_device_info.cpp
    test_software_tare.cpp
    ${MAIN_DIR}/acaia/AcaiaProtocol.cpp
    ${MAIN_DIR}/acaia/DeviceInfo.cpp
    ${MAIN_DIR}/scale/AdvertisingData.cpp
    ${MAIN_DIR}/status/LedPattern.cpp
    ${MAIN_DIR}/scale/drivers/EurekaPrecisa.cpp
    ${UNITY_DIR}/unity.c
)
target_include_directories(host_tests PRIVATE ${MAIN_DIR} ${UNITY_DIR})
target_compile_options(host_tests PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
