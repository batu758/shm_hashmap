#include "shared_memory.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

SharedMemory::SharedMemory()
    : data(nullptr), fd(-1) {}

SharedMemory::~SharedMemory() {
    detach();
}

int SharedMemory::init(const char *name, size_t queue_length, size_t block_size, size_t block_count) {
    if (fd != -1) return 1;

    fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    size_t total_size = sizeof(SharedData)
            + estimate_size_for_task_queue(queue_length)
            + estimate_size_for_block_allocator(block_size, block_count);

    if (ftruncate(fd, static_cast<__off_t>(total_size)) == -1) {
        perror("ftruncate");
        close(fd);
        fd = -1;
        return 1;
    }

    void *ptr = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        fd = -1;
        return 1;
    }

    data = static_cast<SharedData*>(ptr);
    data->size = total_size;
    data->task_queue_offset = sizeof(SharedData);
    data->block_allocator_offset = sizeof(SharedData) + estimate_size_for_task_queue(queue_length);

    get_task_queue()->init(queue_length);
    get_block_allocator()->init(block_size, block_count);

    new (&data->running) std::atomic(true);
    return 0;
}

int SharedMemory::attach(const char *name) {
    if (fd != -1) return -1;

    fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    void *tmp = mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (tmp == MAP_FAILED) {
        perror("mmap");
        close(fd);
        fd = -1;
        return 1;
    }

    size_t size = static_cast<SharedData*>(tmp)->size;
    munmap(tmp, sizeof(SharedData));

    if (size < sizeof(SharedData)) {
        fprintf(stderr, "Invalid shared data header\n");
        close(fd);
        fd = -1;
        return 1;
    }

    void *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        fd = -1;
        return 1;
    }

    data = static_cast<SharedData*>(ptr);
    return 0;
}

void SharedMemory::detach() {
    if (fd == -1) return;

    if (data) {
        munmap(data, data->size);
        data = nullptr;
    }

    close(fd);
    fd = -1;
}

bool SharedMemory::is_running() const {
    return data && data->running.load(std::memory_order_acquire);
}

void SharedMemory::stop() const {
    if (data) {
        data->running.store(false, std::memory_order_release);
    }
}

TaskQueue *SharedMemory::get_task_queue() const {
    if (!data) return nullptr;
    char *ptr = reinterpret_cast<char*>(data) + data->task_queue_offset;
    return reinterpret_cast<TaskQueue*>(ptr);
}

BlockAllocator *SharedMemory::get_block_allocator() const {
    if (!data) return nullptr;
    char *ptr = reinterpret_cast<char*>(data) + data->block_allocator_offset;
    return reinterpret_cast<BlockAllocator*>(ptr);
}
