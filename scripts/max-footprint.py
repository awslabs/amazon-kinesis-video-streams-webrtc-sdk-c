#!/bin/env python3

from typing import List, Callable, Dict, Tuple
import re
import subprocess
from os import path
import os
import time
import signal

ROOT_DIR = os.getcwd()
SNAPSHOT_DELIMITER = "snapshot="
SNAPSHOT_PATTERN = r"""
time=(\d+)
mem_heap_B=(\d+)
mem_heap_extra_B=(\d+)
mem_stacks_B=(\d+)
"""
SNAPSHOT_REGEX = re.compile(SNAPSHOT_PATTERN)
BINARY_NAME_MASTER = "kvsWebrtcClientMaster"
BINARY_NAME_VIEWER = "kvsWebrtcClientViewer"
PROFILE_DURATION_IN_SECONDS = 10
SHUTDOWN_TIMEOUT_IN_SECONDS = 10
OPEN_SOURCE_DIR = path.join(ROOT_DIR, "open-source")


class Snapshot:
    def __init__(self, time: int, mem_heap: int, mem_stacks: int):
        self.time = time
        self.mem_heap = mem_heap
        self.mem_stacks = mem_stacks

    def __repr__(self):
        return f"""
        Time  : {self.time}
        Heap  : {self.mem_heap:,} bytes
        Stacks: {self.mem_stacks:,} bytes
        """


class FootprintResult:
    def __init__(self, max_heap: int, max_stacks: int):
        self.max_heap = max_heap
        self.max_stacks = max_stacks


def parse(massif_path: str) -> List[Snapshot]:
    snapshots: List[Snapshot] = []
    with open(massif_path) as i:
        raw = i.read()

        # The first part will not include memory usage, ignoring.
        #
        # For example:
        #   desc: --stacks=yes
        #   cmd: programs/ssl/mini_client
        #   time_unit: i
        raw_snapshots = raw.split(SNAPSHOT_DELIMITER)[1:]

        for raw_snapshot in raw_snapshots:
            result = SNAPSHOT_REGEX.search(raw_snapshot)
            snapshot = Snapshot(
                int(result.group(1)),
                # result.group(3) = mem_heap_extra. Extra heap can come from the following:
                #  * Every heap block has administrative bytes associated with it
                #  * Allocators often round up the number of bytes asked for to a larger number
                #
                # Therefore, since extra heap is still allocated in the heap, it should be counted
                # as a part of heap allocation.
                int(result.group(2)) + int(result.group(3)),
                int(result.group(4)),
            )
            snapshots.append(snapshot)

    return snapshots


def max_footprint(snapshots: List[Snapshot]) -> FootprintResult:
    return FootprintResult(
        max(snapshots, key=lambda s: s.mem_heap),
        max(snapshots, key=lambda s: s.mem_stacks),
    )


def compile(build_dir: str, **kwargs: Dict[str, str]):
    args = []
    for k, v in kwargs.items():
        args.append(f"-D{k}={v}")
    configure_cmd = ["cmake", ".."] + args
    compile_cmd = ["make", "-j"]
    subprocess.run(configure_cmd, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(compile_cmd, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def run(build_dir: str, channel_name="ScaryTestChannel") -> Tuple[FootprintResult, FootprintResult]:
    valgrind_cmd = ["valgrind", "--tool=massif", "--stacks=yes"]
    master_proc = subprocess.Popen(
        valgrind_cmd + [f"./{BINARY_NAME_MASTER}", channel_name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    viewer_proc = subprocess.Popen(
        valgrind_cmd + [f"./{BINARY_NAME_VIEWER}", channel_name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    time.sleep(PROFILE_DURATION_IN_SECONDS)

    master_proc.send_signal(signal.SIGINT)
    viewer_proc.send_signal(signal.SIGINT)

    master_proc.wait(SHUTDOWN_TIMEOUT_IN_SECONDS)
    viewer_proc.wait(SHUTDOWN_TIMEOUT_IN_SECONDS)

    return (
        max_footprint(parse(f"massif.out.{master_proc.pid}")),
        max_footprint(parse(f"massif.out.{viewer_proc.pid}")),
    )


def compile_and_run(build_dir: str, channel_name="ScaryTestChannel", **kwargs: Dict[str, str]) -> Tuple[FootprintResult, FootprintResult]:
    os.makedirs(build_dir, exist_ok=True)
    os.chdir(build_dir)

    compile(build_dir, **kwargs)
    results = run(build_dir, channel_name)

    os.chdir(ROOT_DIR)
    return results


if __name__ == "__main__":
    results = compile_and_run("build_test")
