#pragma once

#include "battle.h"
#include "board_pins.h"

#ifdef __wasi__
// The laptop preview offers the same practice battle as the device.
bool beginBattleIr();
bool receiveBattleIr(duel::Frame &frame);
void sendBattleIr(duel::Frame frame);
#else
#include <driver/rmt.h>
#include <freertos/ringbuf.h>

namespace {
constexpr rmt_channel_t battleTx = RMT_CHANNEL_0;
constexpr rmt_channel_t battleRx = RMT_CHANNEL_2;
RingbufHandle_t battleRxBuffer = nullptr;

bool nearPulse(uint16_t actual, uint16_t ideal, uint16_t tolerance) {
    return actual >= ideal - tolerance && actual <= ideal + tolerance;
}

bool decodeBattlePulses(const rmt_item32_t *items, size_t count, duel::Frame &frame) {
    if (count != duel::packetBits + 2 || items[0].level0 != 0 || items[0].level1 != 1 ||
        !nearPulse(items[0].duration0, 9000, 1000) ||
        !nearPulse(items[0].duration1, 4500, 800)) return false;

    uint64_t packet = 0;
    for (uint8_t bit = 0; bit < duel::packetBits; ++bit) {
        const rmt_item32_t &item = items[bit + 1];
        if (item.level0 != 0 || item.level1 != 1 || !nearPulse(item.duration0, 560, 230)) {
            return false;
        }
        if (nearPulse(item.duration1, 1690, 320)) packet |= uint64_t(1) << bit;
        else if (!nearPulse(item.duration1, 560, 230)) return false;
    }
    const rmt_item32_t &stop = items[duel::packetBits + 1];
    return stop.level0 == 0 && nearPulse(stop.duration0, 560, 230) &&
           duel::decode(packet, frame);
}
}  // namespace

inline bool beginBattleIr() {
    rmt_config_t tx = RMT_DEFAULT_CONFIG_TX(static_cast<gpio_num_t>(pins::irTransmit), battleTx);
    tx.tx_config.carrier_en = true;
    tx.tx_config.carrier_freq_hz = 38000;
    tx.clk_div = 80;

    rmt_config_t rx = RMT_DEFAULT_CONFIG_RX(static_cast<gpio_num_t>(pins::irReceive), battleRx);
    // At 1 us per tick, allow the 9 ms preamble mark and terminate on a longer idle.
    rx.rx_config.idle_threshold = 10500;

    return rmt_config(&tx) == ESP_OK && rmt_config(&rx) == ESP_OK &&
           rmt_driver_install(battleTx, 0, 0) == ESP_OK &&
           rmt_driver_install(battleRx, 512, 0) == ESP_OK &&
           rmt_get_ringbuf_handle(battleRx, &battleRxBuffer) == ESP_OK &&
           battleRxBuffer != nullptr && rmt_rx_start(battleRx, true) == ESP_OK;
}

inline bool receiveBattleIr(duel::Frame &frame) {
    size_t bytes = 0;
    while (auto *items = static_cast<rmt_item32_t *>(xRingbufferReceive(battleRxBuffer, &bytes, 0))) {
        const bool valid = decodeBattlePulses(items, bytes / sizeof(*items), frame);
        vRingbufferReturnItem(battleRxBuffer, items);
        if (valid) return true;
    }
    return false;
}

inline void sendBattleIr(duel::Frame frame) {
    rmt_item32_t items[duel::packetBits + 2]{};
    const auto burst = [](rmt_item32_t &item, uint16_t mark, uint16_t space) {
        item.level0 = 1;
        item.duration0 = mark;
        item.level1 = 0;
        item.duration1 = space;
    };
    burst(items[0], 9000, 4500);
    const uint64_t packet = duel::encode(frame);
    for (uint8_t bit = 0; bit < duel::packetBits; ++bit) {
        burst(items[bit + 1], 560, (packet >> bit) & 1 ? 1690 : 560);
    }
    burst(items[duel::packetBits + 1], 560, 0);
    rmt_rx_stop(battleRx);
    rmt_write_items(battleTx, items, duel::packetBits + 2, true);
    rmt_rx_start(battleRx, true);
}
#endif
