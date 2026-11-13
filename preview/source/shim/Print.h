#pragma once
#include <stdint.h>
#include <stddef.h>

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t character) = 0;
    size_t print(const char *text) {
        size_t count = 0;
        while (*text) {
            count += write(uint8_t(*text++));
        }
        return count;
    }
    size_t print(char value) { return write(static_cast<uint8_t>(value)); }
    size_t print(uint16_t value) {
        char digits[5];
        size_t length = 0;
        do {
            digits[length++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        size_t written = 0;
        while (length) written += print(digits[--length]);
        return written;
    }
    size_t print(uint8_t value) { return print(static_cast<uint16_t>(value)); }
};
