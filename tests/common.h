#pragma  once

#include "task.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

constexpr size_t MAX_ENTRY_SIZE = 128;

struct PreparedTask {
    TaskType type;
    alignas(Entry) char storage[MAX_ENTRY_SIZE];

    Entry *entry() { return reinterpret_cast<Entry*>(storage); }
    const Entry *entry() const { return reinterpret_cast<const Entry*>(storage); }
};

inline std::vector<PreparedTask> prepare_random_tasks(size_t n) {
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