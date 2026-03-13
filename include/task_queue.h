#pragma once

#include <atomic>

struct alignas(64) Slot {
    std::atomic<size_t> sequence;
    uint32_t block_id;
};

struct TaskQueue {
    uint32_t capacity; // should be power of 2
    alignas(64) std::atomic<size_t> head;
    alignas(64) std::atomic<size_t> tail;
    Slot slots[];

    void init(uint32_t capacity);

    [[nodiscard]] bool push(uint32_t block_id);
    uint32_t pop();
};

size_t estimate_size_for_task_queue(uint32_t capacity);
