#include "shared_memory.h"
#include "task_queue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

void worker_thread(const SharedMemory *shm, const int ops) {
    TaskQueue *queue = shm->get_task_queue();

    for (int i = 0; i < ops; i++) {
        while (!queue->push(i)) {
            std::this_thread::yield();
        }
    }
}

void stress_test(SharedMemory &shm, const int n_threads, const int ops) {
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n_threads; i++) {
        threads.emplace_back(worker_thread, &shm, ops);
    }

    for (auto &t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double ns_per_op = static_cast<double>(duration.count()) / static_cast<double>(n_threads * ops);

    std::cout << "ns per queue op: " << ns_per_op << std::endl;
}

int main() {
    SharedMemory shm;
    const char *name = "/queue_only_memory";

    if (shm.attach(name) != 0) {
        std::cerr << "Failed to attach shared memory\n";
        return 1;
    }

    stress_test(shm, 10, 1000000);

    shm.detach();
}
