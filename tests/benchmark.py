#!/usr/bin/env python3

import subprocess
import time
import json
import re
import signal
import sys

THREADS = [1, 2, 4, 8, 16]
TOTAL_OPS = 1_000_000
OUTFILE = "lattice_results.json"

results = []
server_proc = None


def kill_server():
    global server_proc
    if server_proc and server_proc.poll() is None:
        server_proc.kill()
        server_proc.wait()


def save():
    with open(OUTFILE, "w") as f:
        json.dump(results, f, indent=2)


def interrupt(sig, frame):
    print("\nInterrupted. Current data:\n")
    print(json.dumps(results, indent=2))
    kill_server()
    save()
    sys.exit(0)


signal.signal(signal.SIGINT, interrupt)


def run(server_threads, client_threads):
    global server_proc

    ops_per_thread = TOTAL_OPS // client_threads

    print(f"\nserver threads: {server_threads}\nclient: {client_threads}")

    server_proc = subprocess.Popen(
        ["./build/queue_only_server", "-t", str(server_threads)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    time.sleep(1)

    try:
        output = subprocess.check_output(
            [
                "./build/queue_only_client",
                "-t",
                str(client_threads),
                "-o",
                str(ops_per_thread),
            ],
            text=True,
        )
    finally:
        kill_server()

    match = re.findall(r"ns per query:\s*([0-9.]+)", output)
    if not match:
        print(output.encode('utf8'))
        print("Failed to parse client output")
        return

    ns = float(match[-1])

    print(f"ns/query = {ns}")

    results.append(
        {
            "server_threads": server_threads,
            "client_threads": client_threads,
            "ops_per_thread": ops_per_thread,
            "ns_per_query": ns,
        }
    )

    save()


def main():
    for s in THREADS:
        for c in THREADS:
            run(s, c)

    print("\nFinished\n")
    print(json.dumps(results, indent=2))
    save()


if __name__ == "__main__":
    main()