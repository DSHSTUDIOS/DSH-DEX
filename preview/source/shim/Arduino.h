#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define PROGMEM
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int OUTPUT = 1;
constexpr int INPUT = 0;
constexpr int INPUT_PULLUP = 2;
struct __FlashStringHelper;
struct String {
    const char *value = "";
    size_t length() const { return strlen(value); }
    const char *c_str() const { return value; }
};

inline int constrain(int value, int low, int high) {
    return value < low ? low : value > high ? high : value;
}

uint32_t millis();
int digitalRead(uint8_t pin);
void ledcWriteTone(uint8_t channel, uint32_t hz);
inline void digitalWrite(uint8_t, int) {}
inline void pinMode(uint8_t, int) {}
inline void delay(uint32_t) {}
inline double ledcSetup(uint8_t, double hz, uint8_t) { return hz; }
inline void ledcAttachPin(uint8_t, uint8_t) {}

struct PreviewSerial {
    void begin(unsigned long) {}
    void println(const char *) {}
    void printf(const char *, ...) {}
};
extern PreviewSerial Serial;
