cmake_minimum_required(VERSION 3.13...3.27)

include(pico_sdk_import.cmake)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
project(pico-midi-looper-ble C CXX ASM)
pico_sdk_init()

add_executable(${CMAKE_PROJECT_NAME}
  src/main.c
  src/looper.c
  src/tap_tempo.c
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(${CMAKE_PROJECT_NAME}
  pico_stdlib
  drivers
)
pico_enable_stdio_usb(${CMAKE_PROJECT_NAME} 1)
pico_add_extra_outputs(${CMAKE_PROJECT_NAME})

add_library(drivers
  drivers/ble_midi.c
  drivers/button.c
  drivers/display.c
)
pico_btstack_make_gatt_header(drivers PRIVATE ${CMAKE_CURRENT_LIST_DIR}/midi_service.gatt)
target_include_directories(drivers PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(drivers
  pico_stdlib
  pico_btstack_ble
  pico_btstack_cyw43
  pico_cyw43_arch_none
)
