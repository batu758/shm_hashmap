#include "shared_memory.h"
#include "task_queue.h"
#include "block_allocator.h"
#include "argument_parser.h"

#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>

void submit_task(SharedMemory &shm, TaskType type, const std::string &key, const std::string &value = "") {
    BlockAllocator *allocator = shm.get_block_allocator();
    if (key.size() + value.size() + sizeof(Block) > allocator->block_size) {
        std::cerr << "Query size is too big." << std::endl;
        return;
    }

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
    allocator->deallocate(block_id);
}

void submit_read_bucket(SharedMemory &shm, uint32_t bucket_id) {
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
    task->type = TaskType::READ_BUCKET;
    task->bucket_id = bucket_id;

    while (!queue->push(block_id)) {
        std::cerr << "Couldn't push request retrying." << std::endl;
        std::this_thread::yield();
    }

    uint32_t total = 0;
    while (true) {
        TaskStatus status = task->status.load(std::memory_order_acquire);

        if (status == TaskStatus::WAITING) {
            std::string key(reinterpret_cast<char*>(task->entry.key()), task->entry.key_size);
            std::string val(reinterpret_cast<char*>(task->entry.val()), task->entry.val_size);

            std::cout << "\"" << key << "\": \"" << val << "\"" << std::endl;
            total++;

            task->status.store(TaskStatus::SUBMITTED, std::memory_order_release);
        }
        else if (status == TaskStatus::DONE) {
            if (total == 0)
                std::cout << "(empty)" << std::endl;
            break;
        }
        else if (status == TaskStatus::FAIL) {
            std::cout << "(failed)" << std::endl;
            break;
        }

        std::this_thread::yield();
    }

    allocator->deallocate(block_id);
}

int main(int argc, char *argv[]) {
    constexpr Options CLIENT_OPTIONS =
        Options::NAME;

    Config cfg;
    parse_args(argc, argv, cfg, CLIENT_OPTIONS);

    SharedMemory shm;
    if (shm.attach(cfg.shm_name.c_str()) != 0) {
        std::cerr << "Failed to attach shared memory" << std::endl;
        return 1;
    }

    std::cout << "Enter commands (get/put/insert/update/delete <key> <value?> | read <bucket_id>):" << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd, key, value;
        iss >> cmd >> key;
        if (cmd == "insert" || cmd == "update" || cmd == "put") {
            iss >> value;
        }

        if (cmd == "insert") {
            submit_task(shm, TaskType::INSERT, key, value);
        } else if (cmd == "update") {
            submit_task(shm, TaskType::UPDATE, key, value);
        } else if (cmd == "get") {
            submit_task(shm, TaskType::GET, key);
        } else if (cmd == "put") {
            submit_task(shm, TaskType::PUT, key, value);
        } else if (cmd == "delete") {
            submit_task(shm, TaskType::DELETE, key);
        } else if (cmd == "read") {
            submit_read_bucket(shm, std::stoul(key));
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else {
            std::cerr << "Unknown command: " << cmd << std::endl;
        }
    }

    shm.detach();
    return 0;
}
