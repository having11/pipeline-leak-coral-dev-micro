cmake_minimum_required(VERSION 3.18)

# Toolchain must be set before project() call.
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/coralmicro/cmake/toolchain-arm-none-eabi-gcc.cmake)
endif()

project(pipeline_detection)

if (NOT DEFINED WIFI_SSID)
    set(WIFI_SSID "MyAccessPoint")
endif()
add_definitions(-DWIFI_SSID="${WIFI_SSID}")

# Leave as is for an open AP.
# If a password is specified, we try to connect as WPA2.
if (NOT DEFINED WIFI_PSK)
    set(WIFI_PSK "")
endif()
add_definitions(-DWIFI_PSK="${WIFI_PSK}")

if (NOT DEFINED CORAL_MICRO_ARDUINO)
    set(CORAL_MICRO_ARDUINO 0)
endif()
add_definitions(-DCORAL_MICRO_ARDUINO=${CORAL_MICRO_ARDUINO})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(coralmicro)
add_subdirectory(coralmicro)

include_directories(${PROJECT_SOURCE_DIR}/main/include)

add_executable_m7(pipeline_detection
        ${PROJECT_SOURCE_DIR}/main/src/m7_main.cc
        DATA
        ${PROJECT_SOURCE_DIR}/main/models/yamnet_test_audio.bin>/yamnet_test_audio.bin
        ${PROJECT_SOURCE_DIR}/main/models/yamnet_spectra_in.tflite>/yamnet_spectra_in.tflite
        ${PROJECT_SOURCE_DIR}/main/web/coral_micro_camera.html>/coral_micro_camera.html
        M4_EXECUTABLE
        pipeline_detection_m4
)

target_link_libraries(pipeline_detection
        libs_base-m7_freertos
        libs_base-m7_http_server
        libs_jpeg_m7
        libs_nxp_rt1176-sdk_wiced
        libs_base-m7_wifi
)

add_executable_m4(pipeline_detection_m4
        ${PROJECT_SOURCE_DIR}/main/src/m4_main.cc
)

target_link_libraries(pipeline_detection_m4
        libs_base-m4_freertos
)