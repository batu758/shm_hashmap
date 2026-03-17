import matplotlib.pyplot as plt
import json

with open('results.json', 'r') as f:
    data = json.load(f)

grouped = {}
for entry in data:
    if entry["server_threads"] not in grouped.keys():
        grouped[entry["server_threads"]] = []
    grouped[entry["server_threads"]].append(entry)

plt.figure(figsize=(10, 6))

for server_threads, entries in sorted(grouped.items()):
    entries = sorted(entries, key=lambda x: x["client_threads"])

    x = [e["client_threads"] for e in entries]
    y = [e["ns_per_query"] for e in entries]

    plt.plot(x, y, marker='o', label=f"server_threads={server_threads}")

plt.xlabel("Client Threads")
plt.ylabel("ns per Query")
plt.title("Performance vs Client Threads (by Server Threads)")
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.show()