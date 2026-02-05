#!/usr/bin/env python3
import glob
import os
import re
import statistics
import sys
from collections import defaultdict

from tabulate import tabulate


def parse_profile_logs(log_file):
    stats = defaultdict(list)
    order = {}
    pattern1 = r"PROFILE\s+\w+\(\):\s+\[([^\]]+)\]\s+Time taken:\s+(\d+)\s+ms"
    pattern2 = r"PROFILE\s+\w+\(\):\s+\[([^\]]+)\]\s+(\d+)\s+ms"

    with open(log_file, "r") as f:
        for line in f:
            match = re.search(pattern1, line) or re.search(pattern2, line)
            if match:
                metric, time = match.groups()
                if metric not in order:
                    order[metric] = len(order)
                stats[metric].append(int(time))

    return stats, order


def print_stats(stats, order):
    min_count = int(os.environ.get('MIN_COUNT', '1'))
    rows = []
    for metric in sorted(stats.keys(), key=lambda m: order.get(m, float('inf'))):
        times = stats[metric]
        if len(times) < min_count:
            continue
        avg = sum(times) / len(times)
        stddev = statistics.stdev(times) if len(times) > 1 else 0
        p50 = statistics.quantiles(times, n=100)[49] if len(times) > 1 else times[0]
        p90 = statistics.quantiles(times, n=100)[89] if len(times) > 1 else times[0]
        rows.append([metric, avg, stddev, min(times), p50, p90, max(times), len(times)])

    print(
        tabulate(
            rows,
            headers=["Metric", "Avg", "StdDev", "Min", "P50", "P90", "Max", "Count"],
            tablefmt="grid",
            numalign="left",
            stralign="left",
            floatfmt=".1f",
        )
    )


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: aggregate-profile-stats.py <log_file1> [log_file2 ...]")
        sys.exit(1)

    stats = defaultdict(list)
    order = {}
    log_files = []
    for pattern in sys.argv[1:]:
        matches = glob.glob(pattern)
        if not matches:
            matches = [pattern]
        log_files.extend(matches)
    
    for log_file in log_files:
        file_stats, file_order = parse_profile_logs(log_file)
        for metric, times in file_stats.items():
            if metric not in order:
                order[metric] = len(order)
            stats[metric].extend(times)

    print_stats(stats, order)
