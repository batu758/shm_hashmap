#include "shared_memory.h"
#include "task_queue.h"
#include "hashmap.h"
#include "argument_parser.h"

#include <iostream>
#include <thread>
#include <cstring>
#include <string>


void server_loop(SharedMemory *shm, HashMap *map, int id) {
    TaskQueue *queue = shm->get_task_queue();
    BlockAllocator *allocator = shm->get_block_allocator();

    while (shm->is_running()) {
        uint32_t block_id = queue->pop();

        if (block_id == INVALID_BLOCK) {
            std::this_thread::yield();
            continue;
        }

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
            if (map->remove(&task->entry))
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

int main(int argc, char *argv[]) {
    constexpr Options SERVER_OPTIONS =
        Options::THREADS |
        Options::QUEUE_CAPACITY |
        Options::BLOCK_SIZE |
        Options::BLOCK_COUNT |
        Options::BUCKET_COUNT |
        Options::NAME;

    Config cfg;
    parse_args(argc, argv, cfg, SERVER_OPTIONS);

    SharedMemory shm;
    if (shm.init(cfg.shm_name.c_str(), cfg.queue_cap, cfg.block_size, cfg.block_count) != 0) {
        std::cerr << "Failed to initialize shared memory" << std::endl;
        return 1;
    }

    HashMap map(cfg.bucket_count);

    std::vector<std::thread> threads;
    threads.reserve(cfg.n_threads);
    for (uint32_t i = 0; i < cfg.n_threads; ++i) {
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
