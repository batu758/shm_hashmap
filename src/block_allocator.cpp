#include "block_allocator.h"

void BlockAllocator::init(uint32_t blk_size, uint32_t blk_count) {
    block_size = blk_size;
    block_count = blk_count;

    for (uint32_t i = 0; i < block_count; ++i) {
        get_block(i)->next = (i + 1 < block_count) ? (i + 1) : INVALID_BLOCK;
    }

    // init free head: index=0, tag=0
    new(&free_head) std::atomic<uint64_t>(0);
    free_head.store(0, std::memory_order_release);
}

uint32_t BlockAllocator::allocate() {
    uint64_t old_head = free_head.load(std::memory_order_acquire);

    for (;;) {
        auto old_index = static_cast<uint32_t>(old_head & 0xFFFFFFFF);
        auto tag = static_cast<uint32_t>(old_head >> 32);

        if (old_index == INVALID_BLOCK)
            return INVALID_BLOCK; // no free blocks

        uint32_t new_index = get_block(old_index)->next;
        uint64_t new_head = (static_cast<uint64_t>(tag + 1) << 32) | new_index;

        if (free_head.compare_exchange_weak(
            old_head, new_head,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
            return old_index;
        }
    }
}

void BlockAllocator::deallocate(uint32_t index) {
    if (index >= block_count)
        return;
    uint64_t old_head = free_head.load(std::memory_order_acquire);

    for (;;) {
        auto old_index = static_cast<uint32_t>(old_head & 0xFFFFFFFF);
        auto tag = static_cast<uint32_t>(old_head >> 32);

        get_block(index)->next = old_index;
        uint64_t new_head = (static_cast<uint64_t>(tag + 1) << 32) | index;

        if (free_head.compare_exchange_weak(
            old_head, new_head,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
            return;
        }
    }
}

Block *BlockAllocator::get_block(uint32_t index) {
    if (index >= block_count)
        return nullptr;
    return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(blocks) + index * block_size);
}

size_t estimate_size_for_block_allocator(uint32_t block_size, uint32_t block_count) {
    return sizeof(BlockAllocator) + block_size * block_count;
}