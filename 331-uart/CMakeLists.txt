cmake_minimum_required(VERSION 3.15)
set(PICO_BOARD pico CACHE STRING "Pico board")
include(pico_sdk_import.cmake)

project(331_uart)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_subdirectory(../libs ${CMAKE_BINARY_DIR}/libs)
target_compile_definitions(profiling PUBLIC PROFILING_ENABLED=1)

add_library(platform STATIC platform/platform.c)
target_compile_options(platform PRIVATE -Wall -Wextra)
target_include_directories(platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platform PRIVATE pico_stdlib pico_stdio_usb hardware_adc hardware_dma hardware_uart)

add_library(logic STATIC
    logic/api/api.c
    logic/api/api-callbacks.c
    logic/api/uart-commands.c
    logic/led-task/led-task.c
    logic/button-task/button-task.c
    logic/pi-task/pi-task.c
    logic/edge-task/edge-task.c
    logic/uart-task/uart-task.c
    logic/block-task/block-task.c
)
target_include_directories(logic PUBLIC logic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logic PUBLIC stdio_text_protocol PRIVATE platform systime profiling device)
target_compile_options(logic PRIVATE -Wall -Wextra)

add_executable(${PROJECT_NAME} main.c)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
target_link_libraries(${PROJECT_NAME} PRIVATE logic platform systime profiling logging pico_stdlib)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})
