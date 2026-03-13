#include "shared_memory.h"
#include "task_queue.h"
#include "hashmap.h"

#include <iostream>
#include <thread>
#include <cstring>

void server_loop(SharedMemory *shm, HashMap *map, int id) {
    TaskQueue *queue = shm->get_task_queue();
    BlockAllocator *allocator = shm->get_block_allocator();

    while (shm->is_running()) {
        uint32_t block_id = queue->pop();

        if (block_id == INVALID_BLOCK)
            continue;

        Block *block = allocator->get_block(block_id);
        Task *task = &block->task;
        bool success = false;

        switch (task->type) {
        case TaskType::GET:
            success = map->get(&task->entry);
            break;

        case TaskType::INSERT:
            success = map->insert(&task->entry);
            break;

        case TaskType::UPDATE: // TODO add dedicated function
            map->remove(&task->entry);
            success = map->insert(&task->entry);
            break;

        case TaskType::DELETE:
            success = map->remove(&task->entry);
            break;

        default:
            break;
        }

        task->status.store(success ? TaskStatus::DONE : TaskStatus::FAIL, std::memory_order::release);
    }
}

int main() {
    SharedMemory shm;
    const char *name = "/my_shared_memory";

    uint32_t n_threads = 20;
    uint32_t queue_length = 64;
    uint32_t block_size = 1024;
    uint32_t block_count = 128;
    uint32_t bucket_count = 10000;

    if (shm.init(name, queue_length, block_size, block_count) != 0) {
        std::cerr << "Failed to initialize shared memory" << std::endl;
        return 1;
    }

    HashMap map(bucket_count);

    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (uint32_t i = 0; i < n_threads; ++i) {
        threads.emplace_back(server_loop, &shm, &map, i);
    }

    std::cout << "Server running. Press Enter to stop..." << std::endl;
    std::cin.get();

    shm.stop();
    for (auto &t : threads) {
        t.join();
    }

    shm.detach();
    std::cout << "Server exited successfully." << std::endl;
    map.print_json();
}
