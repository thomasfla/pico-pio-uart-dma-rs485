#include "PicoPioRs485.h"

#include <cstring>

extern "C" {

void rs485_init(RS485_Config *config, const RS485_Config *existing_config) {
    pio_sm_set_pins_with_mask(config->pio, config->sm,
                              (1u << config->rx_pin) | (1u << config->tx_pin) | (1u << config->oe_pin),
                              (1u << config->rx_pin) | (1u << config->tx_pin) | (1u << config->oe_pin));
    pio_sm_set_pindirs_with_mask(config->pio, config->sm,
                                 (0u << config->rx_pin) | (1u << config->tx_pin) | (1u << config->oe_pin),
                                 (1u << config->rx_pin) | (1u << config->tx_pin) | (1u << config->oe_pin));
    pio_gpio_init(config->pio, config->tx_pin);
    pio_gpio_init(config->pio, config->oe_pin);
    pio_gpio_init(config->pio, config->rx_pin);
    gpio_pull_up(config->rx_pin);

    if (existing_config && existing_config->pio == config->pio) {
        config->program_offset = existing_config->program_offset;
    } else {
        config->program_offset = pio_add_program(config->pio, &rs485_program);
    }

    pio_sm_config c = rs485_program_get_default_config(config->program_offset);
    sm_config_set_out_shift(&c, true, false, 32);
    sm_config_set_out_pins(&c, config->tx_pin, 1);
    sm_config_set_in_pins(&c, config->rx_pin);
    sm_config_set_sideset_pins(&c, config->tx_pin);
    sm_config_set_jmp_pin(&c, config->rx_pin);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);
    float div = (float)clock_get_hz(clk_sys) / (8 * config->baud_rate);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(config->pio, config->sm, config->program_offset, &c);
    pio_sm_set_enabled(config->pio, config->sm, true);

    config->dma_channel_tx = dma_claim_unused_channel(true);
    dma_channel_config cfg_tx = dma_channel_get_default_config(config->dma_channel_tx);
    channel_config_set_transfer_data_size(&cfg_tx, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_tx, true);
    channel_config_set_write_increment(&cfg_tx, false);
    channel_config_set_dreq(&cfg_tx, pio_get_dreq(config->pio, config->sm, true));
    dma_channel_configure(config->dma_channel_tx, &cfg_tx,
                          &config->pio->txf[config->sm], config->tx_buffer, 4, false);

    config->dma_channel_rx = dma_claim_unused_channel(true);
    dma_channel_config cfg_rx = dma_channel_get_default_config(config->dma_channel_rx);
    channel_config_set_transfer_data_size(&cfg_rx, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg_rx, false);
    channel_config_set_write_increment(&cfg_rx, true);
    channel_config_set_dreq(&cfg_rx, pio_get_dreq(config->pio, config->sm, false));
    dma_channel_configure(config->dma_channel_rx, &cfg_rx,
                          config->rx_buffer, &config->pio->rxf[config->sm], 1, false);

    channel_config_set_irq_quiet(&cfg_rx, true);
    channel_config_set_irq_quiet(&cfg_tx, true);
    channel_config_set_high_priority(&cfg_rx, true);
    channel_config_set_high_priority(&cfg_tx, true);
}

void rs485_set_baudrate(RS485_Config *config, uint32_t baudrate) {
    config->baud_rate = baudrate;
    float div = (float)clock_get_hz(clk_sys) / (8 * baudrate);
    pio_sm_set_clkdiv(config->pio, config->sm, div);
}

void rs485_send_packet(RS485_Config *config) {
    const uint32_t *ptr = reinterpret_cast<uint32_t *>(config->tx_buffer);
    int len = ptr[0] + 1;

    dma_channel_abort(config->dma_channel_tx);
    dma_channel_abort(config->dma_channel_rx);

    pio_sm_exec_wait_blocking(config->pio, config->sm, pio_encode_jmp(config->program_offset));

    dma_channel_transfer_to_buffer_now(config->dma_channel_rx, config->rx_buffer, sizeof(config->rx_buffer) / 4);
    dma_channel_transfer_from_buffer_now(config->dma_channel_tx, config->tx_buffer, 2 + (len + 3) / 4);
}

bool rs485_prepare_tx_packet(RS485_Config *config, const uint8_t *data, unsigned int length, unsigned int timeout) {
    if ((length + 8 < sizeof(config->tx_buffer)) && (length >= 1)) {
        uint32_t *ptr = reinterpret_cast<uint32_t *>(config->tx_buffer);
        ptr[0] = length - 1;
        ptr[1] = timeout;
        std::memcpy(config->tx_buffer + 8, data, length);
        return true;
    }
    return false;
}

} // extern "C"

PicoPioRs485::PicoPioRs485() : _configured(false) {
    std::memset(&_config, 0, sizeof(_config));
}

bool PicoPioRs485::begin(PIO pio_instance, uint sm_index, uint txPin, uint oePin, uint rxPin, uint32_t baud_rate, PicoPioRs485 *existing) {
    _config.pio = pio_instance;
    _config.sm = sm_index;
    _config.tx_pin = txPin;
    _config.oe_pin = oePin;
    _config.rx_pin = rxPin;
    _config.baud_rate = baud_rate;

    const RS485_Config *existing_config = existing ? &existing->_config : nullptr;
    rs485_init(&_config, existing_config);
    _configured = true;
    return true;
}

void PicoPioRs485::setBaudrate(uint32_t baudrate) {
    if (_configured) {
        rs485_set_baudrate(&_config, baudrate);
    }
}

bool PicoPioRs485::prepareTxPacket(const uint8_t *payload, size_t length, unsigned int timeout_quarters) {
    if (!_configured) {
        return false;
    }
    return rs485_prepare_tx_packet(&_config, payload, static_cast<unsigned int>(length), timeout_quarters);
}

void PicoPioRs485::sendPacket() {
    if (_configured) {
        rs485_send_packet(&_config);
    }
}

int PicoPioRs485::waitForResponse() {
    if (!_configured) {
        return 0;
    }
    return rs485_wait_for_response(&_config);
}

bool PicoPioRs485::responseReady() {
    if (!_configured) {
        return false;
    }
    return rs485_response_ready(&_config);
}

bool PicoPioRs485::txDone() {
    if (!_configured) {
        return false;
    }
    return rs485_tx_done(&_config);
}

void PicoPioRs485::waitTxDone() {
    if (_configured) {
        rs485_wait_tx_done(&_config);
    }
}

uint8_t *PicoPioRs485::rxBuffer() {
    return _config.rx_buffer;
}

const uint8_t *PicoPioRs485::rxBuffer() const {
    return _config.rx_buffer;
}

size_t PicoPioRs485::rxBufferSize() const {
    return sizeof(_config.rx_buffer);
}

RS485_Config &PicoPioRs485::rawConfig() {
    return _config;
}

const RS485_Config &PicoPioRs485::rawConfig() const {
    return _config;
}
