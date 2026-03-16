#pragma once

#include "entry.h"

#include <atomic>

enum class TaskType : uint8_t {
    GET,
    PUT,
    INSERT,
    UPDATE,
    DELETE,
    READ_BUCKET
};

enum class TaskStatus : uint8_t {
    SUBMITTED,
    DONE,
    FAIL,
    WAITING
};

struct Task {
    std::atomic<TaskStatus> status;
    TaskType type;
    union {
        Entry entry;
        uint32_t bucket_id;
    };
};
