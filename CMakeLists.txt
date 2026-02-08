cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(CH32RVD C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(PICO_DEFAULT_WS2812_PIN=23)
pico_sdk_init()

add_executable(ch32v003dbg src/boot.c src/break.c src/console.c src/context.c
  src/flash.c src/main.c src/options.c src/packet.c src/server.c src/swio.c
  src/tusb_config.c src/utils.c src/vendor.c src/xmodem.c)

target_include_directories(ch32v003dbg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  # This directory is required so that TinyUSB can find src/tusb_config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

pico_generate_pio_header(ch32v003dbg ${CMAKE_CURRENT_SOURCE_DIR}/src/singlewire.pio)
pico_enable_stdio_uart(ch32v003dbg 0)
pico_enable_stdio_usb(ch32v003dbg 1)
pico_add_extra_outputs(ch32v003dbg)

target_link_libraries(ch32v003dbg pico_stdlib pico_bootsel_via_double_reset
  pico_status_led hardware_dma hardware_pio tinyusb_device)
