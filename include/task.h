#pragma once

#include "entry.h"

#include <atomic>

enum class TaskType : uint8_t {
    GET,
    INSERT,
    UPDATE,
    DELETE,
};

enum class TaskStatus : uint8_t {
    SUBMITTED,
    DONE,
    FAIL
};

struct Task {
    std::atomic<TaskStatus> status;
    TaskType type;
    Entry entry;
};
