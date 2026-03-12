cmake_minimum_required(VERSION 3.28)
project(hashmap_server)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-O2)

add_executable(hashmap_server
        src/task_queue.cpp
        src/shared_memory.cpp
        src/block_allocator.cpp
        src/hashmap.cpp
        src/server.cpp
)

add_executable(hashmap_client
        src/task_queue.cpp
        src/shared_memory.cpp
        src/block_allocator.cpp
        src/hashmap.cpp
        src/client.cpp
)

target_include_directories(hashmap_server
        PRIVATE ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(hashmap_client
        PRIVATE ${CMAKE_SOURCE_DIR}/include
)
