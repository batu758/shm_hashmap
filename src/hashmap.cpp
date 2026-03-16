#include "hashmap.h"

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

#include "task.h"

// source: https://github.com/opencoff/portable-lib/blob/master/src/hashfunc/fasthash.c
#define mix(h) ({               \
(h) ^= (h) >> 23;               \
(h) *= 0x2127599bf4325c37ULL;   \
(h) ^= (h) >> 47; h; })

uint64_t fasthash64(const void *buf, size_t len, uint64_t seed) {
    const uint64_t m = 0x880355f21e6d1965ULL;
    const uint64_t *pos = (const uint64_t*) buf;
    const uint64_t *end = pos + (len / 8);
    const unsigned char *pos2;
    uint64_t h = seed ^ (len * m);
    uint64_t v;

    for (; pos < end; pos++) {
        v = *pos;
        h ^= mix(v);
        h *= m;
    }

    pos2 = (const unsigned char*) pos;
    v = 0;

    switch (len & 7) {
    case 7: v ^= (uint64_t) pos2[6] << 48; // fallthrough
    case 6: v ^= (uint64_t) pos2[5] << 40; // fallthrough
    case 5: v ^= (uint64_t) pos2[4] << 32; // fallthrough
    case 4: v ^= (uint64_t) pos2[3] << 24; // fallthrough
    case 3: v ^= (uint64_t) pos2[2] << 16; // fallthrough
    case 2: v ^= (uint64_t) pos2[1] << 8; // fallthrough
    case 1: v ^= (uint64_t) pos2[0];
        h ^= mix(v);
        h *= m;
    }

    return mix(h);
}

HashMap::HashMap(size_t bucket_count) : buckets(bucket_count) {}

HashMap::~HashMap() {
    for (auto &bucket : buckets) {
        Node *node = bucket.head;
        while (node) {
            Node *next = node->next;
            std::free(node);
            node = next;
        }
    }
}

size_t HashMap::hash(const uint8_t *data, size_t len) const {
    const uint64_t h = fasthash64(data, len, 1469598103934665603ULL); // random seed
    return h % buckets.size();
}

void HashMap::put(const Entry *entry) {
    Bucket &bucket = buckets[hash(entry->key(), entry->key_size)];
    std::unique_lock lock(bucket.lock);

    Node *cur = bucket.head;
    Node *prev = nullptr;

    while (cur) {
        if (cur->entry.key_size == entry->key_size &&
            std::memcmp(cur->entry.key(), entry->key(), entry->key_size) == 0) {

            // replace existing node
            size_t size = sizeof(Node) + entry->key_size + entry->val_size;
            Node *node = static_cast<Node*>(std::malloc(size));

            node->next = cur->next;
            node->entry.key_size = entry->key_size;
            node->entry.val_size = entry->val_size;

            std::memcpy(node->entry.data, entry->data,
                        entry->key_size + entry->val_size);

            if (prev)
                prev->next = node;
            else
                bucket.head = node;

            std::free(cur);
            return;
            }

        prev = cur;
        cur = cur->next;
    }

    // insert new node
    size_t size = sizeof(Node) + entry->key_size + entry->val_size;
    Node *node = static_cast<Node*>(std::malloc(size));

    node->next = bucket.head;

    node->entry.key_size = entry->key_size;
    node->entry.val_size = entry->val_size;

    std::memcpy(node->entry.data, entry->data,
                entry->key_size + entry->val_size);

    bucket.head = node;
}

bool HashMap::insert(const Entry *entry) {
    Bucket &bucket = buckets[hash(entry->key(), entry->key_size)];
    std::unique_lock lock(bucket.lock);

    Node *cur = bucket.head;

    while (cur) {
        if (cur->entry.key_size == entry->key_size &&
            std::memcmp(cur->entry.key(), entry->key(), entry->key_size) == 0)
            return false;

        cur = cur->next;
    }

    size_t size = sizeof(Node) + entry->key_size + entry->val_size;
    Node *node = static_cast<Node*>(std::malloc(size));
    if (!node)
        return false;

    node->next = bucket.head;

    node->entry.key_size = entry->key_size;
    node->entry.val_size = entry->val_size;

    std::memcpy(node->entry.data, entry->data,
                entry->key_size + entry->val_size);

    bucket.head = node;

    return true;
}

bool HashMap::update(const Entry *entry) {
    Bucket &bucket = buckets[hash(entry->key(), entry->key_size)];
    std::unique_lock lock(bucket.lock);

    Node *cur = bucket.head;
    Node *prev = nullptr;

    while (cur) {
        if (cur->entry.key_size == entry->key_size &&
            std::memcmp(cur->entry.key(), entry->key(), entry->key_size) == 0) {

            size_t size = sizeof(Node) + entry->key_size + entry->val_size;
            Node *node = static_cast<Node*>(std::malloc(size));
            if (!node)
                return false;

            node->next = cur->next;

            node->entry.key_size = entry->key_size;
            node->entry.val_size = entry->val_size;

            std::memcpy(node->entry.data, entry->data,
                        entry->key_size + entry->val_size);

            if (prev)
                prev->next = node;
            else
                bucket.head = node;

            std::free(cur);
            return true;
            }

        prev = cur;
        cur = cur->next;
    }

    return false;
}

bool HashMap::get(Entry *entry) const {
    const Bucket &bucket = buckets[hash(entry->key(), entry->key_size)];
    std::shared_lock lock(bucket.lock);

    Node *cur = bucket.head;

    while (cur) {
        if (cur->entry.key_size == entry->key_size &&
            std::memcmp(cur->entry.key(), entry->key(), entry->key_size) == 0) {
            entry->val_size = cur->entry.val_size;

            std::memcpy(entry->val(),
                        cur->entry.val(),
                        cur->entry.val_size);

            return true;
        }

        cur = cur->next;
    }

    return false;
}

bool HashMap::remove(Entry *entry) {
    Bucket &bucket = buckets[hash(entry->key(), entry->key_size)];
    std::unique_lock lock(bucket.lock);

    Node *cur = bucket.head;
    Node *prev = nullptr;

    while (cur) {
        if (cur->entry.key_size == entry->key_size &&
            std::memcmp(cur->entry.key(), entry->key(), entry->key_size) == 0) {
            if (prev)
                prev->next = cur->next;
            else
                bucket.head = cur->next;

            std::free(cur);
            return true;
        }

        prev = cur;
        cur = cur->next;
    }

    return false;
}

bool HashMap::handle_read_bucket(Task *task) {
    uint32_t bucket_id = task->bucket_id;
    if (bucket_id >= buckets.size())
        return false;

    Bucket &bucket = buckets[bucket_id];
    std::shared_lock lock(bucket.lock);

    Node *cur = bucket.head;

    while (cur) {
        task->entry.key_size = cur->entry.key_size;
        task->entry.val_size = cur->entry.val_size;

        std::memcpy(task->entry.data,
                    cur->entry.data,
                    cur->entry.key_size + cur->entry.val_size);

        task->status.store(TaskStatus::WAITING, std::memory_order_release);

        // wait until client signals to continue
        while (task->status.load(std::memory_order_acquire) != TaskStatus::SUBMITTED) {
            std::this_thread::yield();
        }

        cur = cur->next;
    }

    task->status.store(TaskStatus::DONE, std::memory_order_release);

    return true;
}

void HashMap::print_json() const {
    std::cout << "{";
    bool first = true;

    for (const auto &bucket : buckets) {
        std::shared_lock lock(bucket.lock);
        Node *cur = bucket.head;

        while (cur) {
            if (!first)
                std::cout << ",";
            first = false;

            std::string key(reinterpret_cast<const char*>(cur->entry.key()),
                            cur->entry.key_size);

            std::string val(reinterpret_cast<const char*>(cur->entry.val()),
                            cur->entry.val_size);

            std::cout << "\"" << key << "\":\"" << val << "\"";

            cur = cur->next;
        }
    }

    std::cout << "}" << std::endl;
}