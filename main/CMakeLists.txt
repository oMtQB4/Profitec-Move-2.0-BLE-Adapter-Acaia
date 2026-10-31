idf_component_register(
    SRCS
        "main.cpp"
        "acaia/AcaiaProtocol.cpp"
        "acaia/AcaiaPeripheral.cpp"
        "acaia/DeviceInfo.cpp"
        "bridge/Bridge.cpp"
        "scale/AdvertisingData.cpp"
        "scale/ScaleClient.cpp"
        "scale/drivers/Drivers.cpp"
        "scale/drivers/EurekaPrecisa.cpp"
        "status/LedPattern.cpp"
        "status/StatusLed.cpp"
    INCLUDE_DIRS "."
    REQUIRES bt esp_timer nvs_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
