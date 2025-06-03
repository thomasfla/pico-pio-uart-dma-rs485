add_executable(rs485_example
    main.c
)

target_link_libraries(rs485_example
    pico_stdlib
    pio_rs485_hd
)

pico_enable_stdio_usb(rs485_example 1)
pico_enable_stdio_uart(rs485_example 0)

pico_add_extra_outputs(rs485_example)