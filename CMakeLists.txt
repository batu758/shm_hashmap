cmake_minimum_required(VERSION 3.28)
project(hashmap_server)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-O2)

add_library(hashmap_core
        src/task_queue.cpp
        src/shared_memory.cpp
        src/block_allocator.cpp
        src/hashmap.cpp
        src/argument_parser.cpp
)

target_include_directories(hashmap_core
        PUBLIC ${CMAKE_SOURCE_DIR}/include
)

add_executable(hashmap_server src/server.cpp)
target_link_libraries(hashmap_server PRIVATE hashmap_core)

add_executable(hashmap_client src/client.cpp)
target_link_libraries(hashmap_client PRIVATE hashmap_core)

add_executable(queue_only_server tests/queue_only_server.cpp)
target_link_libraries(queue_only_server PRIVATE hashmap_core)

add_executable(queue_only_client tests/queue_only_client.cpp)
target_link_libraries(queue_only_client PRIVATE hashmap_core)

add_executable(stress_test_client tests/stress_test_client.cpp)
target_link_libraries(stress_test_client PRIVATE hashmap_core)
