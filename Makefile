.PHONY: all build run_server run_client clean

all: build

build:
	cmake -S . -B build
	cmake --build build

run_server: build
	./build/hashmap_server

run_client: build
	./build/hashmap_client

clean:
	rm -rf build
