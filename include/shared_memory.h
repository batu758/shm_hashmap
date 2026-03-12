#pragma once

#include "task_queue.h"
#include "block_allocator.h"

struct SharedData {
    std::atomic<bool> running;
    size_t size;
    size_t task_queue_offset;
    size_t block_allocator_offset;
};

class SharedMemory {
    SharedData *data;
    int fd;

public:
    explicit SharedMemory();
    ~SharedMemory();

    int init(const char *name, size_t queue_length, size_t block_size, size_t block_count);
    int attach(const char *name);
    void detach();

    [[nodiscard]] bool is_running() const;
    void stop() const;

    [[nodiscard]] TaskQueue *get_task_queue() const;
    [[nodiscard]] BlockAllocator *get_block_allocator() const;

    SharedMemory(const SharedMemory &) = delete;
    SharedMemory &operator=(const SharedMemory &) = delete;
};
