#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "rs485.pio.h"
#define pin_rx 1
#define pin_tx 2
#define pin_oe 3 //needs to be equal to pin_tx + 1 (this is because the PIO use sideset for TX and OE)
#define pin_debug 5

void send_packet_rs485(PIO pio, uint sm, int dma_channel_rx, int dma_channel_tx, uint offset, uint8_t * data_rx, uint8_t * data_tx, uint8_t length)
{
    if (length >0)
    {

        // send the length of transfer to the PIO
        //pio_sm_put_blocking(pio, sm, (uint32_t) (length -1));
        // send the time timeout for a stop listening for a response frame (TODO give a formula for this value)
        //pio_sm_put_blocking(pio, sm, (uint32_t) (0xffff));
        //following is the frame data
        dma_channel_abort(dma_channel_tx);
        //Let's restart the SM
        
        pio_sm_clear_fifos(pio, sm);   
        pio_sm_restart(pio, sm);  
        pio_sm_exec(pio, sm, pio_encode_jmp(offset));  
        dma_channel_transfer_from_buffer_now(dma_channel_tx, data_tx, 4);
        dma_channel_abort(dma_channel_rx);
        dma_channel_transfer_to_buffer_now(dma_channel_rx, data_rx, 20);
    }
}


int main() {

    set_sys_clock_khz(100000,true);
    const uint SERIAL_BAUD = 1000000;
    stdio_init_all();

    gpio_init(pin_debug);
    gpio_set_dir(pin_debug, GPIO_OUT);
    gpio_put(pin_debug, 0);
    sleep_ms(500);

    printf("In the main\n");

    // Initialize PIO
    PIO pio = pio0;
    uint sm = 0;
    uint baud = SERIAL_BAUD;
    // Tell PIO to initially drive output-high on the selected pin, then map PIO
    // onto that pin with the IO muxes.
    pio_sm_set_pins_with_mask(pio, sm,    1u<<pin_rx | 1u<<pin_tx | 1u<<pin_oe , 1u<<pin_rx | 1u<<pin_tx | 1u<<pin_oe); 
    pio_sm_set_pindirs_with_mask(pio, sm, 0u<<pin_rx | 1u<<pin_tx | 1u<<pin_oe , 1u<<pin_rx | 1u<<pin_tx | 1u<<pin_oe);

    pio_gpio_init(pio, pin_tx);
    pio_gpio_init(pio, pin_oe);
    pio_gpio_init(pio, pin_rx);
    
    hw_set_bits(&pio->input_sync_bypass, 1u << pin_rx);

    uint offset = pio_add_program(pio, &rs485_program);
    pio_sm_config c = rs485_program_get_default_config(offset);
    
    // OUT shifts to right, no autopull
    sm_config_set_out_shift(&c, true, false, 32);
   // OUT shifts to right, autopull 32
    //sm_config_set_out_shift(&c, true, true, 32);

    // We are mapping both OUT and side-set to the same pin, because sometimes
    // we need to assert user data onto the pin (with OUT) and sometimes
    // assert constant values (start/stop bit)
    sm_config_set_out_pins(&c, pin_tx, 1);
    sm_config_set_in_pins(&c, pin_rx);
    sm_config_set_sideset_pins(&c, pin_tx); 
    sm_config_set_jmp_pin(&c, pin_rx);

    // We only need TX, so get an 8-deep FIFO!
    //sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    //no, keep the fifo TX and RX
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);
    // SM transmits 1 bit per 4 execution cycles.
    float div = (float)clock_get_hz(clk_sys) / (8 * baud);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    
    char data_rx[256] = {0};
    int sizeof_rx = 256;
    char data_tx[256] = {0};

    data_tx[0]=0x05; //size of the data - 1 : 6-1=5
    data_tx[1]=0x00; 
    data_tx[2]=0x00; 
    data_tx[3]=0x00;

    data_tx[4]=0xff; //timeout
    data_tx[5]=0xff; //timeout
    data_tx[6]=0; //timeout
    data_tx[7]=0; //timeout

    data_tx[8]='U';
    data_tx[9]='e';
    data_tx[10]='l';
    data_tx[11]='l';

    data_tx[12]='o';
    data_tx[13]='U';
    data_tx[14]=0;
    data_tx[15]=0;


  

    // Initialize DMA TX
    int dma_channel_tx = dma_claim_unused_channel(true);
    dma_channel_config cfg_tx = dma_channel_get_default_config(dma_channel_tx);
    channel_config_set_transfer_data_size(&cfg_tx, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_tx, true);
    channel_config_set_write_increment(&cfg_tx, false);
    channel_config_set_dreq(&cfg_tx, pio_get_dreq(pio, sm, true)); //DMA wait for pio fifo_TX not to be full
    dma_channel_configure(
        dma_channel_tx, &cfg_tx,
        &pio->txf[sm],  // Destination pointer
        data_tx,        // Source pointer
        4,          // Number of transfers
        false           // do not Start immediately
    );

    // Initialize DMA RX
    int dma_channel_rx = dma_claim_unused_channel(true);
    dma_channel_config cfg_rx = dma_channel_get_default_config(dma_channel_rx);
    channel_config_set_transfer_data_size(&cfg_rx, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_rx, false);
    channel_config_set_write_increment(&cfg_rx, true);
    channel_config_set_dreq(&cfg_rx, pio_get_dreq(pio, sm, false)); //DMA wait for pio fifo_RX not to be full
    dma_channel_configure(
        dma_channel_rx, &cfg_rx,
        data_rx,         // Destination pointer
        &pio->rxf[sm],   // Source pointer
        sizeof_rx,       // Number of transfers
        false            // do not Start immediately
    );


    while (true) {

        gpio_put(pin_debug, 1); //to check that the CPU is free durring transfer
        send_packet_rs485(pio, sm, dma_channel_rx, dma_channel_tx, offset, data_rx, data_tx, 4);
        gpio_put(pin_debug, 0);
        /*if (pio_sm_get_rx_fifo_level(pio,sm))
        {
            printf("in the fifo:");
            while (pio_sm_get_rx_fifo_level(pio,sm))
            {
                printf("%4x ",pio_sm_get_blocking(pio,sm));
            }
            printf("\r\n");

        }*/
        
        printf("dataRX:");
        for (int i = 0 ;i<20;i++)
        {
            printf("%02x ",data_rx[i]);
        }
        printf("\r\n");
        sleep_ms(10);
    }
}