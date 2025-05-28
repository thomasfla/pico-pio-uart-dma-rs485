#include "pico/stdlib.h"
#include <stdio.h>

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "rs485_lib.h"
#include <string.h>
#include <math.h>

#define pin_tx0 0
#define pin_oe0 1 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_rx0 2

#define pin_tx1 3
#define pin_oe1 4 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_rx1 5

#define pin_tx2 6
#define pin_oe2 7 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_rx2 8

#define pin_tx3 9
#define pin_oe3 10 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_rx3 11

int main() {
    set_sys_clock_khz(200*1000,false);
    const uint SERIAL_BAUD =  12500000;

    stdio_init_all();
    sleep_ms(500);

    printf("In the main\n");

    //Configure RS485 for channel 0
    RS485_Config rs485Config0;
    rs485Config0.pio = pio0;  // PIO instance
    rs485Config0.sm = 0;      // State machine index
    rs485Config0.tx_pin = pin_tx0;  // TX pin
    rs485Config0.oe_pin = pin_oe0;  // OE pin
    rs485Config0.rx_pin = pin_rx0;  // RX pin
    rs485Config0.baud_rate = SERIAL_BAUD;  // Baud rate
    rs485_init(&rs485Config0, NULL);

    //Configure RS485 for channel 1
    RS485_Config rs485Config1;
    rs485Config1.pio = pio0;  // PIO instance
    rs485Config1.sm = 1;      // State machine index
    rs485Config1.tx_pin = pin_tx1;  // TX pin
    rs485Config1.oe_pin = pin_oe1;  // OE pin
    rs485Config1.rx_pin = pin_rx1;  // RX pin
    rs485Config1.baud_rate = SERIAL_BAUD;  // Baud rate
    rs485_init(&rs485Config1, &rs485Config0);

    //Print the clock divior for the two state machines (ideally an integer, or high value), 
    //depending on system clock and baudrate, we could have low and non integer value resulting in uart jitter
    printf("divisor0 = %lf\r\n",(float)pio0->sm[0].clkdiv / (1 << 16));
    printf("divisor1 = %lf\r\n",(float)pio0->sm[1].clkdiv / (1 << 16));

    uint8_t data1[10] = {10,11,12,13,14,15,16,17,18,19};
    uint8_t data2[30] = {0};
    for(int i =0; i< sizeof(data2);i++)  data2[i]=i+1;
    while (true) {
        rs485_prepare_tx_packet(&rs485Config0,data1,sizeof(data1));
        rs485_prepare_tx_packet(&rs485Config1,data2,sizeof(data2));
        rs485_send_packet(&rs485Config0);
        rs485_wait_tx_done(&rs485Config0); //Wait TX is done
        //sleep_us(2);
        rs485_send_packet(&rs485Config1);  //Respond (loop back test!)
        //wait for the end of the program (time out or end of RX)
        int lenRx0  = rs485_wait_for_response(&rs485Config0);
        rs485_wait_for_response(&rs485Config1);
        sleep_ms(1000);
        printf("\r\n(RX %d bytes) ",lenRx0);
        for(int i=0;i<lenRx0;i++)
        {
            printf("%02x ", rs485Config0.rx_buffer[i]);
        }
    }
}