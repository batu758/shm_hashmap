#include "shared_memory.h"
#include "task_queue.h"
#include "hashmap.h"

#include <iostream>
#include <thread>
#include <cstring>
#include <string>

// default values
struct Config {
    uint32_t n_threads = 20;
    uint32_t queue_length = 64;
    uint32_t block_size = 1024;
    uint32_t block_count = 64;
    uint32_t bucket_count = 10000;
    std::string shm_name = "/my_shared_memory";
};

void print_usage(const char *name) {
    std::cout << "usage: " << name <<
        " [-h] [-t THREADS] [-q QUEUE_CAPACITY] [-s BLOCK_SIZE] "
        "[-c BLOCK_COUNT] [-b BUCKETS] [-n SHM_NAME]\n";
}

void print_help(const char *name) {
    print_usage(name);
    std::cout <<
        "\n"
        "options:\n"
        "  -t, --threads N       Number of worker threads\n"
        "  -q, --queue N         Task queue capacity\n"
        "  -s, --block-size N    Block size\n"
        "  -c, --block-count N   Block count\n"
        "  -b, --buckets N       Hashmap bucket count\n"
        "  -n, --shm-name NAME   Shared memory name\n"
        "  -h, --help            Show this help\n";
}

void parse_args(int argc, char **argv, Config &cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            cfg.n_threads = std::stoul(argv[++i]);
        } else if ((arg == "-q" || arg == "--queue") && i + 1 < argc) {
            cfg.queue_length = std::stoul(argv[++i]);
        } else if ((arg == "-s" || arg == "--block-size") && i + 1 < argc) {
            cfg.block_size = std::stoul(argv[++i]);
        } else if ((arg == "-c" || arg == "--block-count") && i + 1 < argc) {
            cfg.block_count = std::stoul(argv[++i]);
        } else if ((arg == "-b" || arg == "--buckets") && i + 1 < argc) {
            cfg.bucket_count = std::stoul(argv[++i]);
        } else if ((arg == "-n" || arg == "--shm-name") && i + 1 < argc) {
            cfg.shm_name = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            std::exit(1);
        }
    }
}


void server_loop(SharedMemory *shm, HashMap *map, int id) {
    TaskQueue *queue = shm->get_task_queue();
    BlockAllocator *allocator = shm->get_block_allocator();

    while (shm->is_running()) {
        uint32_t block_id = queue->pop();

        if (block_id == INVALID_BLOCK)
            continue;

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
            map->remove(&task->entry);
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
    Config cfg;
    parse_args(argc, argv, cfg);

    SharedMemory shm;

    if (shm.init(cfg.shm_name.c_str(), cfg.queue_length, cfg.block_size, cfg.block_count) != 0) {
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
