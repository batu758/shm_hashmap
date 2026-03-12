#pragma once

#include "task.h"

#include <atomic>

static constexpr uint32_t INVALID_BLOCK = 0xFFFFFFFF;

struct Block {
    uint32_t next;
    Task task;
};

struct BlockAllocator {
    uint32_t block_size;
    uint32_t block_count;
    std::atomic<uint64_t> free_head; // lower 32 bit: index, upper 32 bit: ABA tag
    Block blocks[];

    void init(uint32_t block_size, uint32_t block_count);
    uint32_t allocate();
    void deallocate(uint32_t index);
    [[nodiscard]] Block *get_block(uint32_t index);
};

size_t estimate_size_for_block_allocator(uint32_t block_size, uint32_t block_count);
