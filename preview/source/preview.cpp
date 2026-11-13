#include "main.cpp"

PreviewSerial Serial;
TwoWire Wire;
SPIClass SPI;

namespace {
uint32_t elapsedMs = 0;
uint32_t buttonMask = 0;
uint32_t toneHz = 0;
uint32_t randomState = 0x62ac497d;
}

uint32_t millis() { return elapsedMs; }
int digitalRead(uint8_t pin) { return (buttonMask & (1u << pin)) ? LOW : HIGH; }
void ledcWriteTone(uint8_t, uint32_t hz) { toneHz = hz; }

uint32_t esp_random() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}

bool beginBattleIr() { return false; }
bool receiveBattleIr(duel::Frame &) { return false; }
void sendBattleIr(duel::Frame) {}

extern "C" {
void preview_setup(uint32_t seed) {
    if (seed) randomState = seed;
    setup();
}
void preview_step(uint32_t now, uint32_t mask) {
    elapsedMs = now;
    buttonMask = mask;
    loop();
}
const uint8_t *preview_buffer() { return screen.getBuffer(); }
unsigned preview_page() { return page; }
unsigned preview_battle() { return static_cast<unsigned>(battle.view); }
uint32_t preview_tone() { return toneHz; }
}
