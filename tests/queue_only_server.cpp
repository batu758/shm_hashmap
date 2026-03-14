#include "shared_memory.h"
#include "task_queue.h"

#include <iostream>
#include <thread>
#include <vector>

void server_loop(SharedMemory *shm, int id) {
    TaskQueue *queue = shm->get_task_queue();

    while (shm->is_running()) {
        uint32_t block_id = queue->pop();

        if (block_id == INVALID_BLOCK) {
            std::this_thread::yield();
        }
    }
}

int main() {
    SharedMemory shm;
    const char *name = "/queue_only_memory";

    uint32_t n_threads = 10;
    uint32_t queue_length = 64;
    uint32_t block_size = 1024;
    uint32_t block_count = 128;

    if (shm.init(name, queue_length, block_size, block_count) != 0) {
        std::cerr << "Failed to initialize shared memory\n";
        return 1;
    }

    std::vector<std::thread> threads;
    threads.reserve(n_threads);

    for (uint32_t i = 0; i < n_threads; ++i) {
        threads.emplace_back(server_loop, &shm, i);
    }

    std::cout << "Queue-only server running. Press Enter to stop...\n";
    std::cin.get();

    shm.stop();

    for (auto &t : threads) {
        t.join();
    }

    shm.detach();
    std::cout << "Server exited successfully\n";
}
