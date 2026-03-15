#include "shared_memory.h"
#include "task_queue.h"
#include "argument_parser.h"

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

void worker_thread(const SharedMemory *shm, const uint32_t ops) {
    TaskQueue *queue = shm->get_task_queue();

    for (int i = 0; i < ops; i++) {
        while (!queue->push(i)) {
            std::this_thread::yield();
        }
    }
}

void stress_test(SharedMemory &shm, const uint32_t n_threads, const uint32_t ops) {
    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < n_threads; i++)
        threads.emplace_back(worker_thread, &shm, ops);

    for (auto &t : threads)
        t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double ns_per_op = static_cast<double>(duration.count()) / static_cast<double>(n_threads * ops);
    double throughput = 1e9 / ns_per_op;

    std::cout << "ns per query: " << ns_per_op << std::endl;
    // std::cout << "ns per queue op: " << ns_per_op << std::endl;
    // std::cout << "throughput: " << static_cast<uint64_t>(throughput) << " op/s" << std::endl;
}

int main(int argc, char *argv[]) {
    constexpr Options CLIENT_OPTIONS =
        Options::NAME |
        Options::THREADS |
        Options::OPERATIONS;

    Config cfg{
        .operations = 1000000,
        .shm_name = "/queue_only_memory"
    };
    parse_args(argc, argv, cfg, CLIENT_OPTIONS);

    SharedMemory shm;
    if (shm.attach(cfg.shm_name.c_str()) != 0) {
        std::cerr << "Failed to attach shared memory\n";
        return 1;
    }

    stress_test(shm, cfg.n_threads, cfg.operations);

    shm.detach();
}
