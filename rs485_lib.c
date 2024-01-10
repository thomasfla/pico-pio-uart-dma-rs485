#include "rs485_lib.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#define pin_debug 12
void rs485_init(RS485_Config *config) {
    // Initialize the RS485 module using the provided config
    // Set up pins, PIO program, DMA channels, etc. based on config values
    pio_sm_set_pins_with_mask(config->pio, config->sm,    1u<<config->rx_pin | 1u<<config->tx_pin | 1u<<config->oe_pin , 1u<<config->rx_pin | 1u<<config->tx_pin | 1u<<config->oe_pin); 
    pio_sm_set_pindirs_with_mask(config->pio, config->sm, 0u<<config->rx_pin | 1u<<config->tx_pin | 1u<<config->oe_pin , 1u<<config->rx_pin | 1u<<config->tx_pin | 1u<<config->oe_pin);
    pio_gpio_init(config->pio, config->tx_pin);
    pio_gpio_init(config->pio, config->oe_pin);
    pio_gpio_init(config->pio, config->rx_pin);
    gpio_pull_up(config->rx_pin); // This is usefull if the RS485 tranceiver output is floating when OE is off (connecting OE and /IE together)
    //hw_set_bits(&config->pio->input_sync_bypass, 1u << config->rx_pin);
    config->program_offset = pio_add_program(config->pio, &rs485_program);
    pio_sm_config c = rs485_program_get_default_config(config->program_offset);
    // OUT shifts to right, no autopull
    sm_config_set_out_shift(&c, true, false, 32);
    // OUT shifts to right, autopull 32
    //sm_config_set_out_shift(&c, true, true, 32);

    // We are mapping both OUT and side-set to the same pin, because sometimes
    // we need to assert user data onto the pin (with OUT) and sometimes
    // assert constant values (start/stop bit)
    sm_config_set_out_pins(&c, config->tx_pin, 1); //out for TX
    sm_config_set_in_pins(&c, config->rx_pin);
    sm_config_set_sideset_pins(&c, config->tx_pin); 
    sm_config_set_jmp_pin(&c, config->rx_pin);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);
    // SM transmits 1 bit per 8 execution cycles.
    float div = (float)clock_get_hz(clk_sys) / (8 * config->baud_rate);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(config->pio, config->sm, config->program_offset, &c); 
    pio_sm_set_enabled(config->pio, config->sm, true);

    // Initialize DMA TX
    config->dma_channel_tx = dma_claim_unused_channel(true);
    dma_channel_config cfg_tx = dma_channel_get_default_config(config->dma_channel_tx);
    channel_config_set_transfer_data_size(&cfg_tx, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_tx, true);
    channel_config_set_write_increment(&cfg_tx, false);
    channel_config_set_dreq(&cfg_tx, pio_get_dreq(config->pio, config->sm, true)); //DMA wait for pio fifo_TX not to be full
    dma_channel_configure(
        config->dma_channel_tx, &cfg_tx,
        &config->pio->txf[config->sm],  // Destination pointer
        config->data_tx,        // Source pointer
        4,          // Number of transfers TODO
        false           // do not Start immediately
    );

    // Initialize DMA RX
    config->dma_channel_rx = dma_claim_unused_channel(true);
    dma_channel_config cfg_rx = dma_channel_get_default_config(config->dma_channel_rx);
    channel_config_set_transfer_data_size(&cfg_rx, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_rx, false);
    channel_config_set_write_increment(&cfg_rx, true);
    channel_config_set_dreq(&cfg_rx, pio_get_dreq(config->pio, config->sm, false)); //DMA wait for pio fifo_RX not to be full
    dma_channel_configure(
        config->dma_channel_rx, &cfg_rx,
        config->data_rx,         // Destination pointer
        &config->pio->rxf[config->sm],   // Source pointer
        1,       // Number of transfers 
        false            // do not Start immediately
    );

    channel_config_set_irq_quiet	(&cfg_rx,true);
    channel_config_set_irq_quiet	(&cfg_tx,true);

    channel_config_set_high_priority(&cfg_rx,true);
    channel_config_set_high_priority(&cfg_tx,true);

}
void send_packet_rs485(RS485_Config *config) {
    uint32_t *ptr = (uint32_t *)config->data_tx;
    int len = ptr[0]+1;  
    // send the length of transfer to the PIO
    //pio_sm_put_blocking(pio, sm, (uint32_t) (length -1));
    // send the time timeout for a stop listening for a response frame (TODO give a formula for this value)
    //pio_sm_put_blocking(pio, sm, (uint32_t) (0xffff));
    //following is the frame data
    //pio_sm_exec_wait_blocking (config->pio, config->sm, pio_encode_jmp(config->program_offset+31)); //Force SM to be at the end

    //pio_sm_set_enabled(config->pio, config->sm,false);

    dma_channel_abort(config->dma_channel_tx);
    dma_channel_abort(config->dma_channel_rx);

    pio_sm_exec_wait_blocking(config->pio, config->sm, pio_encode_jmp(config->program_offset)); //State machine set to waiting state

    //gpio_put(pin_debug, 0);
    dma_channel_transfer_to_buffer_now(config->dma_channel_rx, config->data_rx, config->buffer_size_rx/4);
    dma_channel_transfer_from_buffer_now(config->dma_channel_tx, config->data_tx, 2+len/4+len%4);

    //pio_sm_set_enabled(config->pio, config->sm,true);
    //Manual transfer
    /*
    int j = 0;
    int j_sup = 2+len/4+len%4;

    for(j=0;j<j_sup;j++)
    {
        pio_sm_put_blocking(config->pio, config->sm, ptr[j]);
        //printf("%x ",ptr[j]);
    }    
    //dma_channel_wait_for_finish_blocking(config->dma_channel_tx);
    */
    //pio_sm_exec(config->pio, config->sm, pio_encode_jmp(config->program_offset+1)); //Activate state machine

    //sleep_ms(1);
    
}

void construct_packet_rs485(RS485_Config *config, uint8_t* data, unsigned int length)
{
    if ((length + 8 < config->buffer_size_rx) && (length >= 1))
    {
        // 32-bit assignment using uint32_t
        uint32_t *ptr = (uint32_t *)config->data_tx;
        ptr[0]=length-1;   //Fill the length for PIO
        ptr[1]=1000; //Fill the timeout (Unit is quarter of bits period) at 12.5Mbauds it is 0.02us / LSB
        memcpy(config->data_tx+8,data,length);
    }
}

