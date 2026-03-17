#pragma once

#include <cstdint>
#include <string>

enum class Options : uint32_t {
    NONE            = 0,
    THREADS         = 1 << 0,
    QUEUE_CAPACITY  = 1 << 1,
    BLOCK_SIZE      = 1 << 2,
    BLOCK_COUNT     = 1 << 3,
    BUCKET_COUNT    = 1 << 4,
    NAME            = 1 << 5,
    OPERATIONS      = 1 << 6
};

constexpr Options operator|(Options a, Options b) {
    return static_cast<Options>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr bool has_option(Options options, Options option) {
    return (static_cast<uint32_t>(options) & static_cast<uint32_t>(option)) != 0;
}

// default values
struct Config {
    uint32_t n_threads = 20;
    uint32_t queue_cap = 64;
    uint32_t block_size = 1024;
    uint32_t block_count = 64;
    uint32_t bucket_count = 10000;
    uint32_t operations = 10000;
    std::string shm_name = "/my_shared_memory";
};

void parse_args(int argc, char **argv, Config &cfg, Options options);
