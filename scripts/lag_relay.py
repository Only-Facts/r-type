#!/usr/bin/env python3
import argparse
import heapq
import random
import select
import socket
import time


def parse_endpoint(text):
    host, port = text.rsplit(":", 1)
    return host, int(port)


def parse_args():
    parser = argparse.ArgumentParser(
        description="UDP relay between R-Type clients and the server that adds latency, jitter, "
                    "reordering and loss, to test the client interpolation on a bad connection.")
    parser.add_argument("--listen", type=int, default=4243, help="port the clients connect to (default: 4243)")
    parser.add_argument("--server", default="127.0.0.1:4242", help="server address (default: 127.0.0.1:4242)")
    parser.add_argument("--delay", type=float, default=30.0, help="base one-way delay in ms (default: 30)")
    parser.add_argument("--jitter", type=float, default=60.0, help="random extra delay in ms, 0 to this value (default: 60)")
    parser.add_argument("--reorder", type=float, default=0.1, help="probability that a packet is held back (default: 0.1)")
    parser.add_argument("--reorder-delay", type=float, default=40.0, help="extra delay of a held back packet in ms (default: 40)")
    parser.add_argument("--loss", type=float, default=0.0, help="probability that a packet is dropped (default: 0)")
    parser.add_argument("--direction", choices=["both", "down", "up"], default="down",
                        help="which traffic is impaired: server to client (down), client to server (up) or both "
                             "(default: down)")
    parser.add_argument("--seed", type=int, default=None, help="random seed, to replay the same conditions")
    return parser.parse_args()


class LagRelay:
    def __init__(self, args):
        self.args = args
        self.server = parse_endpoint(args.server)
        self.rng = random.Random(args.seed)
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.listener.bind(("0.0.0.0", args.listen))
        self.listener.setblocking(False)
        self.upstreams = {}
        self.clients = {}
        self.queue = []
        self.order = 0
        self.stats = {"forwarded": 0, "reordered": 0, "dropped": 0}

    def impaired(self, downstream):
        if self.args.direction == "both":
            return True
        return (self.args.direction == "down") == downstream

    def schedule(self, sock, data, address, downstream):
        delay = 0.0
        if self.impaired(downstream):
            if self.rng.random() < self.args.loss:
                self.stats["dropped"] += 1
                return
            delay = (self.args.delay + self.rng.uniform(0.0, self.args.jitter)) / 1000.0
            if self.rng.random() < self.args.reorder:
                delay += self.args.reorder_delay / 1000.0
                self.stats["reordered"] += 1
        self.order += 1
        heapq.heappush(self.queue, (time.monotonic() + delay, self.order, sock, data, address))

    def upstream_for(self, client):
        sock = self.upstreams.get(client)
        if sock is None:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(("0.0.0.0", 0))
            sock.setblocking(False)
            self.upstreams[client] = sock
            self.clients[sock] = client
            print(f"client {client[0]}:{client[1]} -> server via port {sock.getsockname()[1]}", flush=True)
        return sock

    def read(self, sock):
        while True:
            try:
                data, address = sock.recvfrom(65535)
            except (BlockingIOError, ConnectionResetError):
                return
            if sock is self.listener:
                self.schedule(self.upstream_for(address), data, self.server, downstream=False)
            else:
                self.schedule(self.listener, data, self.clients[sock], downstream=True)

    def flush(self):
        now = time.monotonic()
        while self.queue and self.queue[0][0] <= now:
            _, _, sock, data, address = heapq.heappop(self.queue)
            try:
                sock.sendto(data, address)
                self.stats["forwarded"] += 1
            except OSError:
                self.stats["dropped"] += 1

    def run(self):
        args = self.args
        print(f"lag relay on port {args.listen} -> {self.server[0]}:{self.server[1]}  "
              f"delay {args.delay:g} ms + jitter 0-{args.jitter:g} ms, reorder {args.reorder:.0%} "
              f"(+{args.reorder_delay:g} ms), loss {args.loss:.0%}, direction {args.direction}", flush=True)
        next_report = time.monotonic() + 5.0
        while True:
            timeout = 0.05
            if self.queue:
                timeout = max(0.0, min(timeout, self.queue[0][0] - time.monotonic()))
            readable, _, _ = select.select([self.listener, *self.upstreams.values()], [], [], timeout)
            for sock in readable:
                self.read(sock)
            self.flush()
            if time.monotonic() >= next_report:
                next_report += 5.0
                print(f"forwarded {self.stats['forwarded']}  reordered {self.stats['reordered']}  "
                      f"dropped {self.stats['dropped']}", flush=True)


if __name__ == "__main__":
    try:
        LagRelay(parse_args()).run()
    except KeyboardInterrupt:
        pass
