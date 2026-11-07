# One file per scale driver, picked up without editing this list. ESP-IDF also
# evaluates this file in script mode, where CONFIGURE_DEPENDS is not allowed:
# after adding a driver file run `idf.py reconfigure` (or `make fullclean build`).
file(GLOB SCALE_DRIVER_SRCS "${CMAKE_CURRENT_LIST_DIR}/scale/drivers/*.cpp")

idf_component_register(
    SRCS
        "main.cpp"
        "acaia/AcaiaProtocol.cpp"
        "acaia/AcaiaPeripheral.cpp"
        "acaia/DeviceInfo.cpp"
        "bridge/Bridge.cpp"
        "config/ConfigProtocol.cpp"
        "config/ConfigService.cpp"
        "config/SettingsStore.cpp"
        "ota/AppImage.cpp"
        "ota/AppValidity.cpp"
        "ota/EspFlashTarget.cpp"
        "ota/OtaProtocol.cpp"
        "ota/OtaService.cpp"
        "ota/OtaSession.cpp"
        "ota/UpdateMode.cpp"
        "scale/AdvertisingData.cpp"
        "scale/Driver.cpp"
        "scale/ScaleClient.cpp"
        "status/LedPattern.cpp"
        "status/StatusLed.cpp"
        ${SCALE_DRIVER_SRCS}
    INCLUDE_DIRS "."
    REQUIRES app_update bt esp_app_format esp_driver_gpio esp_timer nvs_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
