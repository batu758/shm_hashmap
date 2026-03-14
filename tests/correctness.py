import string
import subprocess
import random
import json
import time

predefined_commands = [
    "insert a 1",
    "insert b 2",
    "insert c 3",
    "update b 22",
    "get a",
    "delete c",
    "get b",
    "exit"
]

class Hashmap:
    def __init__(self):
        self.hashmap = {}

    def execute(self, line):
        parts = line.strip().split()

        if not parts:
            return None

        cmd = parts[0]

        if cmd == "insert":
            return self.insert(parts[1], parts[2])

        if cmd == "update":
            return self.update(parts[1], parts[2])

        if cmd == "get":
            return self.get(parts[1])

        if cmd == "delete":
            return self.remove(parts[1])

        if cmd == "exit":
            return None

        return None

    def get(self, key):
        return self.hashmap.get(key, '(not found)')

    def insert(self, key, value):
        if self.hashmap.get(key) is None:
            self.hashmap[key] = value
            return '(success)'
        else:
            return '(failed)'

    def update(self, key, value):
        if self.hashmap.get(key) is None:
            return '(failed)'
        else:
            self.hashmap[key] = value
            return '(success)'

    def remove(self, key):
        try:
            self.hashmap.pop(key)
            return '(success)'
        except:
            return '(failed)'


def random_key(length=5):
    return ''.join(random.choices(string.ascii_lowercase, k=length))

def random_value(length=5):
    return ''.join(random.choices(string.ascii_lowercase + string.digits, k=length))

def generate_random_commands(n):
    commands = []
    known_keys = set()

    for _ in range(n):
        op = random.choice(["insert", "update", "get"])

        if known_keys and random.random() < 0.7:
            key = random.choice(list(known_keys))
        else:
            key = random_key()

        if op == "insert":
            value = random_value()
            commands.append(f"insert {key} {value}")
            known_keys.add(key)

        elif op == "update":
            value = random_value()
            commands.append(f"update {key} {value}")

        elif op == "get":
            commands.append(f"get {key}")

        elif op == "delete":
            commands.append(f"delete {key}")
            known_keys.discard(key)

    commands.append("exit")
    return commands


def run_test():
    server = subprocess.Popen(
        ["./build/hashmap_server"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    time.sleep(1)

    client = subprocess.Popen(
        ["./build/hashmap_client"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    client.stdout.readline()

    hashmap = Hashmap()

    for i, cmd in enumerate(generate_random_commands(1000)):
        client.stdin.write(cmd + "\n")
        client.stdin.flush()

        expected = hashmap.execute(cmd)
        if expected is not None:
            output = client.stdout.readline().strip()

            if output != expected:
                print(f"❌ {i} Mismatch for '{cmd}'")
                print(f"Expected: {expected}")
                print(f"Got     : {output}")
            else:
                print(f"✅ {i} {cmd} -> {output}")

    client.stdin.close()
    client.wait()

    server.stdin.write("\n")
    server.stdin.flush()
    stdout, stderr = server.communicate()
    server.wait()

    lines = stdout.strip().split("\n")
    hashmap_json_line = lines[-1]

    try:
        final_hashmap = json.loads(hashmap_json_line)
    except json.JSONDecodeError:
        print("Failed to parse server JSON output:")
        print(hashmap_json_line)
        return

    if final_hashmap != hashmap.hashmap:
        print("\n❌ Final hashmap mismatch")
        print("Expected:", hashmap.hashmap)
        print("Got     :", final_hashmap)
    else:
        print("\n✅ Final hashmap correct")

    #print("\nServer final hashmap:")
    #print(json.dumps(final_hashmap, indent=2))

if __name__ == "__main__":
    run_test()
