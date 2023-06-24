include_directories(${PROJECT_SOURCE_DIR}/main/include)

set(model_name yamnet_spectra_in.tflite)

file(GLOB all_m7_src
        "${PROJECT_SOURCE_DIR}/main/include/*.h"
        "${PROJECT_SOURCE_DIR}/main/src/m7_*.cc"
)

file(GLOB all_m4_src
        "${PROJECT_SOURCE_DIR}/main/include/*.h"
        "${PROJECT_SOURCE_DIR}/main/src/m4_*.cc"
)

add_executable_m7(pipeline_detection
        ${all_m7_src}
        DATA
        ${PROJECT_SOURCE_DIR}/main/models/yamnet_test_audio.bin>/yamnet_test_audio.bin
        ${PROJECT_SOURCE_DIR}/main/models/${model_name}>/${model_name}
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
        ${all_m4_src}
)

target_include_directories(pipeline_detection_m4 PUBLIC
    ${PROJECT_SOURCE_DIR}/coralmicro/third_party/tflite-micro
)

target_link_libraries(pipeline_detection_m4
        libs_base-m4_freertos
)