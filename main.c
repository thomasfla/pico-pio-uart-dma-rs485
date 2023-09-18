#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "rs485.pio.h"
#define pin_tx 2
#define pin_oe 3
#define pin_debug 5



void send_packet_rs485(PIO pio, uint sm, int dma_channel, uint offset, uint8_t * data, uint8_t length)
{
    if (length >0)
    {
        pio_sm_restart(pio, sm);
        pio_sm_exec(pio, sm, pio_encode_jmp(offset));
        // send the length of transfer to the PIO
        pio_sm_put_blocking(pio, sm, (uint32_t) (length -1));
        //following is the DATA
        dma_channel_transfer_from_buffer_now( dma_channel, data, length);
        //dma_channel_start(dma_channel);
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
    pio_sm_set_pins_with_mask(pio, sm, 3u << pin_tx, 3u << pin_tx);
    pio_sm_set_pindirs_with_mask(pio, sm, 3u << pin_tx , 3u << pin_tx);
    pio_gpio_init(pio, pin_tx);
    pio_gpio_init(pio, pin_oe);
    uint offset = pio_add_program(pio, &rs485_program);
    pio_sm_config c = rs485_program_get_default_config(offset);
    
    // OUT shifts to right, no autopull
    sm_config_set_out_shift(&c, true, false, 32);

    // We are mapping both OUT and side-set to the same pin, because sometimes
    // we need to assert user data onto the pin (with OUT) and sometimes
    // assert constant values (start/stop bit)
    sm_config_set_out_pins(&c, pin_tx, 1);
    sm_config_set_sideset_pins(&c, pin_tx); 


    // We only need TX, so get an 8-deep FIFO!
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // SM transmits 1 bit per 4 execution cycles.
    float div = (float)clock_get_hz(clk_sys) / (4 * baud);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    
    // Initialize DMA
    char data[256] = {0};
    data[0]=0x55;
    data[1]=0x55;
    data[2]=0x55;
    int len = 10;

    int dma_channel = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_channel);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(pio, sm, true)); //DMA wait for pio fifo_TX not to be full
    dma_channel_configure(
        dma_channel, &cfg,
        &pio->txf[sm],  // Destination pointer
        data,           // Source pointer
        len+1,          // Number of transfers
        false           // do not Start immediately
    );

    while (true) {

        gpio_put(pin_debug, 1);
        send_packet_rs485(pio, sm,dma_channel,offset, data,len);
        gpio_put(pin_debug, 0);
        sleep_ms(300);
    }



}

