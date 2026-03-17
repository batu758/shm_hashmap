# Concurrent HashMap Server & Client with Shared Memory

## Overview

This project implements a concurrent hashmap server and a client that communicates with the server via POSIX shared memory which supports get, put, insert, update, delete and read bucket operations. The server supports insert, read, update, and delete operations on a hashmap, using linked lists for bucket collisions. The hashmap allows concurrent accesses using read-write locks per bucket. Shared memory acts as a lock-free message buffer, which consists of a Vyukov MPMC queue `TaskQueue` and fixed size block allocator `BlockAllocator`.

## Building

Requirements: C++20, Make, CMake
```
make
```

## Running
```
make run_server
make run_client
```
Actual executables are generated in `build` folder and can be directly executed.
```
./build/hashmap_server
./build/hashmap_client
```

## Implementation

### Server
- Initializes a hashmap in its own memory space and a shared memory space for communication
- Receives tasks from task queue and executes the task which provides input and output buffers
- Writes `DONE` or `FAIL` to task status to indicate task is done and can be read by clients

### Client
- Connects to the server using POSIX shm.
- Sends tasks using this process:
    - allocate block in shared memory using `BlockAllocator`
    - write task data
    - put block_id in `TaskQueue`
    - wait until task status changes
    - free block

## TODO:
- [x] argument parsing
- [x] add multiple threads to server
- [x] stress tests / benchmarks
- [x] correctness
- [x] graceful termination for server
- [x] dedicated update, put, read bucket functions in hashmap
- [x] client checks for possible buffer overflows
- [x] markdown file for design choices, evaluation etc.
