#pragma once
#include <stdint.h>
#include <stddef.h>

class TwoWire {
public:
    bool begin(int = -1, int = -1, uint32_t = 100000) { return true; }
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t address) { currentAddress = address; count = 0; }
    size_t write(uint8_t value) {
        if (count++ == 0) registerAddress = value;
        return 1;
    }
    uint8_t endTransmission(bool = true) { return currentAddress == 0x3C || currentAddress == 0x18 ? 0 : 2; }
    size_t requestFrom(uint8_t address, size_t length, bool) {
        index = 0;
        return address == 0x18 ? length : 0;
    }
    int read() {
        const size_t byte = index++;
        if (registerAddress == 0x0F) return 0x33;
        return byte == 5 ? 0x40 : 0;  // Fixed 1 g on Z.
    }
private:
    uint8_t currentAddress = 0;
    uint8_t registerAddress = 0;
    size_t count = 0;
    size_t index = 0;
};
extern TwoWire Wire;
