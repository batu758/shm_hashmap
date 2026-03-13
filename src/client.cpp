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

void submit_task(SharedMemory &shm, TaskType type, const std::string &key, const std::string &value = "") {
    BlockAllocator *allocator = shm.get_block_allocator();
    TaskQueue *queue = shm.get_task_queue();

    uint32_t block_id = allocator->allocate();
    if (block_id == INVALID_BLOCK) {
        std::cerr << "No free blocks available." << std::endl;
        return;
    }

    Block *block = allocator->get_block(block_id);
    Task *task = &block->task;

    task->status.store(TaskStatus::SUBMITTED, std::memory_order_relaxed);
    task->type = type;
    task->entry.key_size = key.size();
    task->entry.val_size = value.size();
    std::memcpy(task->entry.data, key.data(), key.size());
    if (!value.empty()) {
        std::memcpy(task->entry.data + key.size(), value.data(), value.size());
    }

    while (!queue->push(block_id)) {
        std::cerr << "Couldn't push request retrying." << std::endl;
        std::this_thread::yield();
    }

    TaskStatus status;
    while ((status = task->status.load(std::memory_order_acquire)) == TaskStatus::SUBMITTED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (type == TaskType::GET) {
        if (status == TaskStatus::DONE) {
            std::string val(reinterpret_cast<char*>(block->task.entry.val()), block->task.entry.val_size);
            std::cout << val << std::endl;
        } else {
            std::cout << "(not found)" << std::endl;
        }
    } else {
        if (status == TaskStatus::DONE) {
            std::cout << "(success)" << std::endl;
        } else {
            std::cout << "(failed)" << std::endl;
        }
    }
}

int main() {
    SharedMemory shm;
    const char *name = "/my_shared_memory";

    if (shm.attach(name) != 0) {
        std::cerr << "Failed to attach shared memory" << std::endl;
        return 1;
    }

    std::cout << "Enter commands (insert/update/get/delete <key> <value?>):" << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd, key, value;
        iss >> cmd >> key;
        if (cmd == "insert" || cmd == "update") {
            iss >> value;
        }

        if (cmd == "insert") {
            submit_task(shm, TaskType::INSERT, key, value);
        } else if (cmd == "update") {
            submit_task(shm, TaskType::UPDATE, key, value);
        } else if (cmd == "get") {
            submit_task(shm, TaskType::GET, key);
        } else if (cmd == "delete") {
            submit_task(shm, TaskType::DELETE, key);
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else {
            std::cerr << "Unknown command: " << cmd << std::endl;
        }
    }

    shm.detach();
    return 0;
}