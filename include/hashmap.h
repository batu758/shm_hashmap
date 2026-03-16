#pragma once

#include "entry.h"

#include <cstdint>
#include <shared_mutex>
#include <vector>

#include "task.h"

class HashMap {
    struct Node {
        Node *next;
        Entry entry;
    };

    struct Bucket {
        Node *head = nullptr;
        mutable std::shared_mutex lock;
    };

    std::vector<Bucket> buckets;

    size_t hash(const uint8_t *data, size_t len) const;

public:
    explicit HashMap(size_t bucket_count);
    ~HashMap();

    void put(const Entry *entry);
    bool insert(const Entry *entry);
    bool update(const Entry *entry);
    bool get(Entry *entry) const;
    bool remove(Entry *entry);
    bool handle_read_bucket(Task *task);
    void print_json() const;

    HashMap(const HashMap &) = delete;
    HashMap &operator=(const HashMap &) = delete;
};
