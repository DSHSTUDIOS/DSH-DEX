#pragma once
#include <stdint.h>
struct SPIClass {
    void begin() {}
    uint8_t transfer(uint8_t value) { return value; }
};
extern SPIClass SPI;
