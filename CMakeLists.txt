# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")
cmake_minimum_required(VERSION 3.13)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(pico_pio_uart_dma_rs485)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(pico_pio_uart_dma_rs485
    main.c rs485_lib.c
)


pico_generate_pio_header(pico_pio_uart_dma_rs485 ${CMAKE_CURRENT_LIST_DIR}/rs485.pio)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(pico_pio_uart_dma_rs485 pico_stdlib hardware_pio hardware_dma)

# use USB Uart
pico_enable_stdio_usb(pico_pio_uart_dma_rs485 1)
pico_enable_stdio_uart(pico_pio_uart_dma_rs485 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(pico_pio_uart_dma_rs485)
