#include "block_allocator.h"
#include "argument_parser.h"
#include "hashmap.h"
#include "common.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>

void execute_task(HashMap &map, PreparedTask &prepared_task) {
    Entry *entry = prepared_task.entry();
    switch (prepared_task.type) {
    case TaskType::GET:
        map.get(entry);
        break;

    case TaskType::INSERT:
        map.insert(entry);
        break;

    case TaskType::UPDATE:
        map.remove(entry);
        map.insert(entry);
        break;

    case TaskType::DELETE:
        map.remove(entry);
        break;

    default:
        break;
    }
}

void local_worker(HashMap *map, std::vector<PreparedTask> *tasks, size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        execute_task(*map, (*tasks)[start + i]);
    }
}

void local_stress_test(HashMap &map, std::vector<PreparedTask> &tasks, size_t n_threads, size_t ops_per_thread) {
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n_threads; i++) {
        threads.emplace_back(
            local_worker,
            &map,
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

    std::cout << "Local - ns per query: " << ns_per_query << std::endl;
}

int main(int argc, char *argv[]) {
    constexpr Options CLIENT_OPTIONS =
        Options::BUCKET_COUNT |
        Options::THREADS |
        Options::OPERATIONS;

    Config cfg{
        .operations = 1000000,
    };
    parse_args(argc, argv, cfg, CLIENT_OPTIONS);

    HashMap map(cfg.bucket_count);
    auto tasks = prepare_random_tasks(cfg.n_threads * cfg.operations);

    local_stress_test(map, tasks, cfg.n_threads, cfg.operations);
    // local_stress_test(map, tasks, cfg.n_threads, cfg.operations);

    return 0;
}
