include_directories(${PROJECT_SOURCE_DIR}/main/include)

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
        M4_EXECUTABLE
        pipeline_detection_m4
)

target_link_libraries(pipeline_detection
        libs_base-m7_freertos
)

add_executable_m4(pipeline_detection_m4
        ${all_m4_src}
)

target_link_libraries(pipeline_detection_m4
        libs_base-m4_freertos
)