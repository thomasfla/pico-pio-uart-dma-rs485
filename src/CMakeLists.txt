add_library(pio_rs485_hd INTERFACE)

target_sources(pio_rs485_hd INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pio_rs485_hd.c
)

target_include_directories(pio_rs485_hd INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_generate_pio_header(pio_rs485_hd ${CMAKE_CURRENT_LIST_DIR}/pio_rs485_hd.pio)

target_link_libraries(pio_rs485_hd INTERFACE
    hardware_pio
    hardware_dma
    hardware_irq
    hardware_gpio
)