#pragma once

#include <Arduino.h>
#include <driver/rmt.h>
#include <freertos/ringbuf.h>

namespace infrared {

struct Fighter {
    uint8_t dex;
    uint8_t level;
    uint8_t attack;
};

inline bool same(Fighter a, Fighter b) {
    return a.dex == b.dex && a.level == b.level && a.attack == b.attack;
}

constexpr uint8_t garchomp = 1;
constexpr uint8_t pikachu = 2;
constexpr uint8_t charizard = 3;
constexpr uint8_t giratina = 4;
constexpr uint8_t metagross = 5;

inline const char *name(uint8_t dex) {
    switch (dex) {
        case garchomp: return "Garchomp";
        case pikachu: return "Pikachu";
        case charizard: return "Charizard";
        case giratina: return "Giratina";
        case metagross: return "Metagross";
        default: return nullptr;
    }
}

inline uint8_t checksum(uint32_t bits, bool ack) {
    uint8_t crc = ack ? 0x2a : 0xd5;
    for (unsigned i = 0; i < 3; ++i) {
        crc ^= static_cast<uint8_t>(bits >> (i * 8));
        for (unsigned b = 0; b < 8; ++b) {
            crc = static_cast<uint8_t>((crc << 1) ^ ((crc & 0x80) ? 0x07 : 0));
        }
    }
    return crc;
}

inline uint32_t pack(Fighter fighter, bool ack) {
    const uint32_t data = uint32_t(fighter.dex) | (uint32_t(fighter.level) << 8) |
                          (uint32_t(fighter.attack) << 16);
    return data | (uint32_t(checksum(data, ack)) << 24);
}

inline bool unpack(uint32_t bits, Fighter &fighter, bool &ack) {
    const uint8_t crc = bits >> 24;
    ack = crc == checksum(bits, true);
    if (!ack && crc != checksum(bits, false)) return false;
    fighter = {uint8_t(bits), uint8_t(bits >> 8), uint8_t(bits >> 16)};
    return name(fighter.dex) && fighter.level >= 1 && fighter.level <= 50 &&
           fighter.attack >= 1 && fighter.attack <= 150;
}

class Link {
public:
    bool begin(uint8_t txPin, uint8_t rxPin) {
        rmt_config_t tx = RMT_DEFAULT_CONFIG_TX(static_cast<gpio_num_t>(txPin), txChannel);
        tx.tx_config.carrier_en = true;
        tx.tx_config.carrier_freq_hz = 38000;

        rmt_config_t rx = RMT_DEFAULT_CONFIG_RX(static_cast<gpio_num_t>(rxPin), rxChannel);
        rx.rx_config.idle_threshold = 11000;
        return rmt_config(&tx) == ESP_OK && rmt_config(&rx) == ESP_OK &&
               rmt_driver_install(txChannel, 0, 0) == ESP_OK &&
               rmt_driver_install(rxChannel, 512, 0) == ESP_OK &&
               rmt_get_ringbuf_handle(rxChannel, &buffer) == ESP_OK && buffer &&
               rmt_rx_start(rxChannel, true) == ESP_OK;
    }

    void poll() {
        if (sending && rmt_wait_tx_done(txChannel, 0) == ESP_OK) {
            sending = false;
            rmt_rx_start(rxChannel, true);
        }
    }

    bool receive(Fighter &fighter, bool &ack) {
        if (sending) return false;
        size_t bytes = 0;
        while (auto *items = static_cast<rmt_item32_t *>(xRingbufferReceive(buffer, &bytes, 0))) {
            uint32_t bits = 0;
            const bool valid = decode(items, bytes / sizeof(*items), bits) &&
                               unpack(bits, fighter, ack);
            vRingbufferReturnItem(buffer, items);
            if (valid) return true;
        }
        return false;
    }

    bool send(Fighter fighter, bool ack) {
        if (sending) return false;
        const uint32_t bits = pack(fighter, ack);
        pulse(pulses[0], 9000, 4500);
        for (uint8_t i = 0; i < 32; ++i) {
            pulse(pulses[i + 1], 560, bits & (uint32_t(1) << i) ? 1690 : 560);
        }
        pulse(pulses[33], 560, 0);
        rmt_rx_stop(rxChannel);
        sending = rmt_write_items(txChannel, pulses, 34, false) == ESP_OK;
        if (!sending) rmt_rx_start(rxChannel, true);
        return sending;
    }

private:
    static constexpr rmt_channel_t txChannel = RMT_CHANNEL_0;
    static constexpr rmt_channel_t rxChannel = RMT_CHANNEL_2;
    RingbufHandle_t buffer = nullptr;
    bool sending = false;
    rmt_item32_t pulses[34]{};

    static void pulse(rmt_item32_t &item, uint16_t mark, uint16_t space) {
        item.level0 = 1;
        item.duration0 = mark;
        item.level1 = 0;
        item.duration1 = space;
    }

    static bool within(int measured, int nominal, int tolerance) {
        return abs(measured - nominal) <= tolerance;
    }

    static bool decode(const rmt_item32_t *items, size_t count, uint32_t &bits) {
        if (count != 34 || items[0].level0 || !items[0].level1 ||
            !within(items[0].duration0, 9000, 1000) ||
            !within(items[0].duration1, 4500, 800)) return false;
        for (uint8_t i = 0; i < 32; ++i) {
            const auto &item = items[i + 1];
            if (item.level0 || !item.level1 || !within(item.duration0, 560, 235)) return false;
            if (within(item.duration1, 1690, 325)) bits |= uint32_t(1) << i;
            else if (!within(item.duration1, 560, 235)) return false;
        }
        return !items[33].level0 && within(items[33].duration0, 560, 235);
    }
};

}  // namespace infrared
