#include "task_queue.h"

#include <thread>
#include <immintrin.h>

#include "block_allocator.h"

constexpr size_t SPIN_LIMIT = 64;

inline size_t backoff(size_t spin) {
    if (spin < SPIN_LIMIT) {
        ++spin;
        std::atomic_signal_fence(std::memory_order_seq_cst); // prevents compiler reordering
    } else {
        std::this_thread::yield();
    }
    return spin;
}

void TaskQueue::init(uint32_t cap) {
    capacity = cap;
    new(&head) std::atomic<size_t>(0);
    new(&tail) std::atomic<size_t>(0);

    for (size_t i = 0; i < capacity; i++) {
        new(&slots[i].sequence) std::atomic(i);
    }
}

void TaskQueue::push(uint32_t block_id) {
    size_t ticket = tail.fetch_add(1, std::memory_order_relaxed);
    size_t idx = ticket & (capacity - 1);
    Slot* slot = &slots[idx];

    size_t spin = 0;
    for (;;) {
        size_t seq = slot->sequence.load(std::memory_order_acquire);
        if (seq == ticket) break;
        spin = backoff(spin);
    }

    slot->block_id = block_id;
    slot->sequence.store(ticket + 1, std::memory_order_release);
}

uint32_t TaskQueue::pop() {
    size_t ticket = head.fetch_add(1, std::memory_order_relaxed);
    size_t idx = ticket & (capacity - 1);
    Slot* slot = &slots[idx];

    size_t spin = 0;
    for (;;) {
        size_t seq = slot->sequence.load(std::memory_order_acquire);
        if (seq == ticket + 1) break;
        std::this_thread::yield();
        spin = backoff(spin);
    }

    uint32_t block_id = slot->block_id;
    slot->sequence.store(ticket + capacity, std::memory_order_release);

    return block_id;
}

// TODO
uint32_t TaskQueue::try_popping() {
    return INVALID_BLOCK;
}

size_t estimate_size_for_task_queue(uint32_t capacity) {
    return sizeof(TaskQueue) + capacity * sizeof(Slot);
}
