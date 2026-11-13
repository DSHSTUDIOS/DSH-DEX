#pragma once
#include <stdint.h>
#include <stddef.h>

extern "C" {
uint32_t preview_load();
void preview_save(uint32_t value);
}

class Preferences {
public:
    bool begin(const char *, bool) { return true; }
    bool isKey(const char *) { return preview_load() != UINT32_MAX; }
    uint32_t getUInt(const char *, uint32_t fallback) {
        const uint32_t value = preview_load();
        return value == UINT32_MAX ? fallback : value;
    }
    size_t putUInt(const char *, uint32_t value) {
        preview_save(value);
        return sizeof(value);
    }
};
