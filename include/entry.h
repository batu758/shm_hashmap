#pragma once

#include <cstdint>

struct Entry {
    uint32_t key_size;
    uint32_t val_size;
    uint8_t data[];

    uint8_t* key() { return data; }
    [[nodiscard]] const uint8_t* key() const { return data; }

    uint8_t* val() { return data + key_size; }
    [[nodiscard]] const uint8_t* val() const { return data + key_size; }
};