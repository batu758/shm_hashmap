#include "shared_memory.h"
#include "task_queue.h"
#include "argument_parser.h"

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

int main(int argc, char *argv[]) {
    constexpr Options SERVER_OPTIONS =
        Options::THREADS |
        Options::QUEUE_CAPACITY |
        Options::NAME;

    Config cfg {
        .block_size = sizeof(Block),
        .block_count = 1,
        .shm_name = "/queue_only_memory",
    };
    parse_args(argc, argv, cfg, SERVER_OPTIONS);

    SharedMemory shm;
    if (shm.init(cfg.shm_name.c_str(), cfg.queue_cap, cfg.block_size, cfg.block_count) != 0) {
        std::cerr << "Failed to initialize shared memory" << std::endl;
        return 1;
    }

    std::vector<std::thread> threads;
    threads.reserve(cfg.n_threads);
    for (uint32_t i = 0; i < cfg.n_threads; ++i) {
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
