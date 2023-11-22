#pragma once

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "rs485.pio.h"
#include "hardware/clocks.h"
// Define your default configuration here
#define DEFAULT_PIN_TX 0
#define DEFAULT_PIN_OE 1
#define DEFAULT_PIN_RX 2
#define DEFAULT_BAUD_RATE 12000000

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
    uint8_t *data_rx;              // Receive buffer
    uint8_t *data_tx;              // Transmit buffer
    size_t buffer_size_rx;         // Size of RX buffer
    size_t buffer_size_tx;         // Size of TX buffer

} RS485_Config;
void rs485_init(RS485_Config *config);
void send_packet_rs485(RS485_Config *config);
void construct_packet_rs485(RS485_Config *config, uint8_t* payload, unsigned int length);