cmake_minimum_required(VERSION 3.1)
project(desktop_c C)

set(PWAOS_SDK_DIR "$ENV{PWAOS_SDK_DIR}" CACHE STRING "Path to pwaos_sdk runtime directory")
if(NOT PWAOS_SDK_DIR)
    message(FATAL_ERROR "PWAOS_SDK_DIR environment variable is not set")
endif()
add_definitions(-DPWAOS_SDK_DIR="${PWAOS_SDK_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(${PWAOS_SDK_DIR}/include)
link_directories(${PWAOS_SDK_DIR}/lib/linux)

add_executable(desktop desktop_app.c)
target_link_libraries(desktop PRIVATE pwaos_runtime_api pthread dl)

add_custom_command(TARGET desktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PWAOS_SDK_DIR}/lib/linux/libpwaos_runtime_api.so"
        "$<TARGET_FILE_DIR:desktop>"
    COMMENT "Copy libpwaos_runtime_api.so to output directory"
)
