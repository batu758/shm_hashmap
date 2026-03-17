#include "shared_memory.h"
#include "task_queue.h"
#include "block_allocator.h"
#include "argument_parser.h"
#include "hashmap.h"
#include "common.h"

#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

void submit_prepared_task(const SharedMemory &shm, const PreparedTask &prepared, uint32_t block_id) {
    BlockAllocator *allocator = shm.get_block_allocator();
    TaskQueue *queue = shm.get_task_queue();

    Block *block = allocator->get_block(block_id);
    Task *task = &block->task;

    const Entry *src = prepared.entry();
    Entry *dst = &task->entry;

    task->status.store(TaskStatus::SUBMITTED, std::memory_order_relaxed);
    task->type = prepared.type;

    dst->key_size = src->key_size;
    dst->val_size = src->val_size;

    std::memcpy(
        dst->data,
        src->data,
        src->key_size + src->val_size
    );

    while (!queue->push(block_id)) {
        std::this_thread::yield();
    }

    while (task->status.load(std::memory_order_acquire) == TaskStatus::SUBMITTED) {
        std::this_thread::yield();
    }
}

void worker(const SharedMemory *shm, const std::vector<PreparedTask> *tasks, size_t start, size_t count) {
    BlockAllocator *allocator = shm->get_block_allocator();

    uint32_t block_id = allocator->allocate();
    while (block_id == INVALID_BLOCK) {
        block_id = allocator->allocate();
        std::this_thread::yield();
    }

    for (size_t i = 0; i < count; i++) {
        submit_prepared_task(*shm, (*tasks)[start + i], block_id);
    }

    allocator->deallocate(block_id);
}

void stress_test(SharedMemory &shm, const std::vector<PreparedTask> &tasks, size_t n_threads, size_t ops_per_thread) {
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n_threads; i++) {
        threads.emplace_back(
            worker,
            &shm,
            &tasks,
            i * ops_per_thread,
            ops_per_thread
        );
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    float ns_per_query = static_cast<float>(duration.count()) / static_cast<float>(n_threads * ops_per_thread);

    std::cout << "Remote - ns per query: " << ns_per_query << std::endl;
}


int main(int argc, char *argv[]) {
    constexpr Options CLIENT_OPTIONS =
        Options::NAME |
        Options::THREADS |
        Options::OPERATIONS;

    Config cfg{
        .operations = 1000000,
    };
    parse_args(argc, argv, cfg, CLIENT_OPTIONS);

    SharedMemory shm;
    if (shm.attach(cfg.shm_name.c_str()) != 0) {
        std::cerr << "Failed to attach shared memory" << std::endl;
        return 1;
    }

    auto tasks = prepare_random_tasks(cfg.n_threads * cfg.operations);

    stress_test(shm, tasks, cfg.n_threads, cfg.operations);
    // stress_test(shm, tasks, cfg.n_threads, cfg.operations);

    shm.detach();
    return 0;
}
