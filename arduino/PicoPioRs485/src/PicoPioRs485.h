#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "pio_rs485_hd.pio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_PIN_TX 0
#define DEFAULT_PIN_OE 1
#define DEFAULT_PIN_RX 2
#define DEFAULT_BAUD_RATE 12000000
#define RS485_MAX_BUFFER 128

typedef struct {
    PIO pio;
    uint sm;
    int dma_channel_tx;
    int dma_channel_rx;
    uint tx_pin;
    uint oe_pin;
    uint rx_pin;
    uint baud_rate;
    uint program_offset;
    uint8_t tx_buffer[RS485_MAX_BUFFER];
    uint8_t rx_buffer[RS485_MAX_BUFFER];
} RS485_Config;

void rs485_init(RS485_Config *config, const RS485_Config *existing_config);
void rs485_set_baudrate(RS485_Config *config, uint32_t baudrate);
void rs485_send_packet(RS485_Config *config);
bool rs485_prepare_tx_packet(RS485_Config *config, const uint8_t *payload, unsigned int length, unsigned int timeout);

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

#ifdef __cplusplus
}

class PicoPioRs485 {
public:
    PicoPioRs485();

    bool begin(PIO pio_instance, uint sm_index, uint txPin, uint oePin, uint rxPin, uint32_t baud_rate, PicoPioRs485 *existing = nullptr);
    void setBaudrate(uint32_t baudrate);
    bool prepareTxPacket(const uint8_t *payload, size_t length, unsigned int timeout_quarters);
    void sendPacket();
    int waitForResponse();
    bool responseReady();
    bool txDone();
    void waitTxDone();
    uint8_t *rxBuffer();
    const uint8_t *rxBuffer() const;
    size_t rxBufferSize() const;
    RS485_Config &rawConfig();
    const RS485_Config &rawConfig() const;

private:
    RS485_Config _config;
    bool _configured;
};

#endif
