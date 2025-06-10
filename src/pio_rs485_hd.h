#pragma once

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "pio_rs485_hd.pio.h"
#include "hardware/clocks.h"
// Define your default configuration here
#define DEFAULT_PIN_TX 0
#define DEFAULT_PIN_OE 1
#define DEFAULT_PIN_RX 2
#define DEFAULT_BAUD_RATE 12000000

#define RS485_MAX_BUFFER 128

typedef struct {
    PIO pio;                       // PIO instance
    uint sm;                       // State machine index
    int dma_channel_tx;            // DMA channel for TX
    int dma_channel_rx;            // DMA channel for RX
    uint tx_pin;                   // GPIO pin for TX
    uint oe_pin;                   // GPIO pin for OE
    uint rx_pin;                   // GPIO pin for RX
    uint baud_rate;                // Baud rate for communication
    uint program_offset;           // Offset of the PIO program
    uint8_t tx_buffer[RS485_MAX_BUFFER];
    uint8_t rx_buffer[RS485_MAX_BUFFER];

} RS485_Config;

void rs485_init(RS485_Config *config, RS485_Config *existing_config);

void rs485_send_packet(RS485_Config *config);

bool rs485_prepare_tx_packet(RS485_Config *config, uint8_t* payload, unsigned int length, unsigned int timeout);

static inline bool rs485_response_ready(RS485_Config *config) {
    return pio_sm_get_pc(config->pio, config->sm) == (config->program_offset + 31);
}

static inline int rs485_wait_for_response(RS485_Config *config) {
    while (!rs485_response_ready(config)) {
        tight_loop_contents();
    }
    uint words_remaining = dma_channel_hw_addr(config->dma_channel_rx)->transfer_count;
    return sizeof(config->rx_buffer) - (words_remaining * 4);
}

static inline bool rs485_tx_done(RS485_Config *config) {
    return pio_sm_get_pc(config->pio, config->sm) >= (config->program_offset + 13);
}


static inline void rs485_wait_tx_done(RS485_Config *config) {
    while (!rs485_tx_done(config)) {
        tight_loop_contents();
    }
}