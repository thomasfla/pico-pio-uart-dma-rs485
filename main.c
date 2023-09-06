#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "rs485.pio.h"
#define pin_tx 2
int main() {

    set_sys_clock_khz(125000,true);
    const uint SERIAL_BAUD = 12500000;
    stdio_init_all();

    

    // Initialize PIO
    PIO pio = pio0;
    uint sm = 0;
    uint baud = SERIAL_BAUD;
    // Tell PIO to initially drive output-high on the selected pin, then map PIO
    // onto that pin with the IO muxes.
    pio_sm_set_pins_with_mask(pio, sm, 1u << pin_tx, 1u << pin_tx);
    pio_sm_set_pindirs_with_mask(pio, sm, 1u << pin_tx, 1u << pin_tx);
    pio_gpio_init(pio, pin_tx);

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

    // SM transmits 1 bit per 8 execution cycles.
    float div = (float)clock_get_hz(clk_sys) / (8 * baud);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    
    
    
    
    // Initialize DMA
    char *data = "?abcdef";
    data[0] = 6;  // Number of bytes to transmit
    int dma_channel = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_channel);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);

    dma_channel_configure(
        dma_channel, &cfg,
        &pio->txf[sm],  // Destination pointer
        data,           // Source pointer
        data[0],        // Number of transfers
        true            // Start immediately
    );

    uint32_t end_addr = (uint32_t) (data + data[0]);  // Calculate the end address of the data buffer

    while (true) {
        // Start DMA transfer
        dma_channel_start(dma_channel);

        // Poll the DMA register to check for completion
        while (dma_hw->ch[dma_channel].al1_read_addr != end_addr) {
            tight_loop_contents();
        }

        // Optionally, you can reset the DMA channel for the next iteration here

        // Sleep for 10 milliseconds
        sleep_ms(10);
    }
}

