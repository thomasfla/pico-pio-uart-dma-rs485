#include "pico/stdlib.h"
#include <stdio.h>

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "rs485_lib.h"
#include <string.h>
#define pin_tx0 0
#define pin_oe0 1 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_rx0 2
/*
#define pin_tx1 3
#define pin_oe1 4 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_rx1 5

#define pin_tx2 6
#define pin_oe2 7 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_rx2 8

#define pin_tx2 9
#define pin_oe2 10 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_rx2 11
*/
#define pin_debug 12

int main() {

    set_sys_clock_khz(120000,true);
    const uint SERIAL_BAUD = 12000000;
    stdio_init_all();

    gpio_init(pin_debug);
    gpio_set_dir(pin_debug, GPIO_OUT);
    gpio_put(pin_debug, 0);
    sleep_ms(500);

    printf("In the main\n");

    //Configure RS485 for channel 0
    RS485_Config rs485Config;
    rs485Config.pio = pio0;  // PIO instance
    rs485Config.sm = 0;      // State machine index
    rs485Config.tx_pin = pin_tx0;  // TX pin
    rs485Config.oe_pin = pin_oe0;  // OE pin
    rs485Config.rx_pin = pin_rx0;  // RX pin
    rs485Config.baud_rate = SERIAL_BAUD;  // Baud rate
    uint8_t data_rx[256] = {0};
    uint8_t data_tx[256] = {0};
    rs485Config.data_rx = data_rx;
    rs485Config.data_tx = data_tx;
    rs485Config.buffer_size_rx = sizeof(data_rx);
    rs485Config.buffer_size_tx = sizeof(data_tx);
    rs485_init(&rs485Config);
    
    uint8_t* str = "Hello World!";
    construct_packet_rs485(&rs485Config,str,strlen(str));
/*     data_tx[0]=0x05; //size of the data - 1 : 6-1=5
    data_tx[1]=0x00; 
    data_tx[2]=0x00; 
    data_tx[3]=0x00; 

    data_tx[4]=0xff; //timeout
    data_tx[5]=0xff; //timeout
    data_tx[6]=0xff; //timeout
    data_tx[7]=0xff; //timeout

    data_tx[8]='H';
    data_tx[9]='e'; 
    data_tx[10]='l';
    data_tx[11]='l';
    data_tx[12]='o';
    data_tx[13]='!'; */

    while (true) {

        printf("dataRX:");
        for (int i = 0 ;i<20;i++)
        {
            printf("%02x ",data_rx[i]);
        }
        printf("RX DMA transfer_count = %d", dma_channel_hw_addr(rs485Config.dma_channel_rx)->transfer_count);
        printf("\r\n");
        gpio_put(pin_debug, 1); //to check that the CPU is free durring transfer
        send_packet_rs485(&rs485Config);
        gpio_put(pin_debug, 0);
        sleep_ms(10);
    }
}