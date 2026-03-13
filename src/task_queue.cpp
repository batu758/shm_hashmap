#include "task_queue.h"

#include <thread>
#include <immintrin.h>

#include "block_allocator.h"

void TaskQueue::init(uint32_t cap) {
    capacity = cap;
    new(&head) std::atomic<size_t>(0);
    new(&tail) std::atomic<size_t>(0);

    for (size_t i = 0; i < capacity; i++) {
        new(&slots[i].sequence) std::atomic(i);
    }
}

bool TaskQueue::push(uint32_t block_id) {
    Slot *slot;
    size_t pos = tail.load(std::memory_order_relaxed);

    for (;;) {
        slot = &slots[pos & (capacity - 1)];

        size_t seq = slot->sequence.load(std::memory_order_acquire);
        intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

        if (dif == 0) {
            if (tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                break;
        } else if (dif < 0) {
            // queue full
            return false;
        } else {
            pos = tail.load(std::memory_order_relaxed);
        }
    }

    slot->block_id = block_id;
    slot->sequence.store(pos + 1, std::memory_order_release);

    return true;
}

uint32_t TaskQueue::pop() {
    Slot *slot;
    size_t pos = head.load(std::memory_order_relaxed);

    for (;;) {
        slot = &slots[pos & (capacity - 1)];

        size_t seq = slot->sequence.load(std::memory_order_acquire);
        intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

        if (dif == 0) {
            if (head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                break;
        } else if (dif < 0) {
            // queue empty
            return INVALID_BLOCK;
        } else {
            pos = head.load(std::memory_order_relaxed);
        }
    }

    uint32_t block_id = slot->block_id;
    slot->sequence.store(pos + capacity, std::memory_order_release);

    return block_id;
}

size_t estimate_size_for_task_queue(uint32_t capacity) {
    return sizeof(TaskQueue) + capacity * sizeof(Slot);
}
