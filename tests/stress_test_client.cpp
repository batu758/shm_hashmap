#include "shared_memory.h"
#include "task_queue.h"
#include "block_allocator.h"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

#include "hashmap.h"

constexpr size_t MAX_ENTRY_SIZE = 128;

struct PreparedTask {
    TaskType type;
    alignas(Entry) char storage[MAX_ENTRY_SIZE];

    Entry *entry() { return reinterpret_cast<Entry*>(storage); }
    const Entry *entry() const { return reinterpret_cast<const Entry*>(storage); }
};

std::vector<PreparedTask> prepare_random_tasks(size_t n) {
    std::vector<PreparedTask> tasks;
    tasks.resize(n);

    for (size_t i = 0; i < n; i++) {
        PreparedTask &t = tasks[i];
        Entry *entry = t.entry();

        int op = rand() % 4;

        switch (op) {
        case 0:
            t.type = TaskType::INSERT;
            break;
        case 1:
            t.type = TaskType::UPDATE;
            break;
        case 2:
            t.type = TaskType::GET;
            break;
        default:
            t.type = TaskType::DELETE;
            break;
        }

        std::string key = "key_" + std::to_string(rand() % 100000);
        std::string value;

        if (t.type == TaskType::INSERT || t.type == TaskType::UPDATE)
            value = "val_" + std::to_string(rand() % 100000);

        entry->key_size = key.size();
        entry->val_size = value.size();

        std::memcpy(entry->data, key.data(), key.size());

        if (!value.empty()) {
            std::memcpy(entry->data + key.size(), value.data(), value.size());
        }
    }

    return tasks;
}

void submit_prepared_task(const SharedMemory &shm, const PreparedTask &prepared) {
    BlockAllocator *allocator = shm.get_block_allocator();
    TaskQueue *queue = shm.get_task_queue();

    uint32_t block_id = allocator->allocate();
    while (block_id == INVALID_BLOCK) {
        block_id = allocator->allocate();
    }

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

    allocator->deallocate(block_id);
}

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

void worker(const SharedMemory *shm, const std::vector<PreparedTask> *tasks, size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        submit_prepared_task(*shm, (*tasks)[start + i]);
    }
}

void local_worker(HashMap *map, std::vector<PreparedTask> *tasks, size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        execute_task(*map, (*tasks)[start + i]);
    }
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

    std::cout << "Local  - ns per query: " << ns_per_query << std::endl;
}

int main() {
    SharedMemory shm;
    const char *name = "/my_shared_memory";

    if (shm.attach(name) != 0) {
        std::cerr << "Failed to attach shared memory" << std::endl;
        return 1;
    }

    HashMap map(10000);
    constexpr size_t n_threads = 10;
    constexpr size_t ops_per_thread = 10000;

    auto tasks = prepare_random_tasks(n_threads * ops_per_thread);

    stress_test(shm, tasks, n_threads, ops_per_thread);
    stress_test(shm, tasks, n_threads, ops_per_thread);
    local_stress_test(map, tasks, n_threads, ops_per_thread);
    local_stress_test(map, tasks, n_threads, ops_per_thread);

    shm.detach();
    return 0;
}
