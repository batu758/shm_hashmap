#pragma once

#include "entry.h"

#include <cstdint>
#include <shared_mutex>
#include <vector>

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

    bool insert(const Entry *entry);
    bool get(Entry *entry) const;
    bool remove(Entry *entry);

    HashMap(const HashMap &) = delete;
    HashMap &operator=(const HashMap &) = delete;
};
