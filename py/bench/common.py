"""
Definitions shared by the xv6 and Linux benchmark harnesses.
"""

import argparse
import csv
import json
import os
import platform
import statistics
import subprocess
import sys
import time
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

from custom_logger import LoggerSetup  # noqa: E402

VCPUS = 8

FIELDS = [
    "sched",
    "test",
    "threads",
    "rep",
    "prompt_tokens",
    "pos_final",
    "sampled_tokens",
    "ttft_ms",
    "rate_pos_per_s",
    "inference_ms",
    "e2e_ms",
    "peak_ram_mb",
]


def load_spec():
    """
    Load the test definitions shared by both harnesses.

    Returns:
        dict: prompts, tests, thread counts, repetitions, seed and model name.
    """
    with open(os.path.join(HERE, "tests.json")) as spec_file:
        return json.load(spec_file)


def host_meta(host_cores, memory):
    """
    Describe the machine a benchmark ran on.

    Args:
        host_cores: taskset core list used for QEMU, or None.
        memory: guest memory size passed to QEMU.

    Returns:
        dict: host platform, QEMU version, vCPU count, guest memory and pinned cores.
    """
    version = subprocess.run(
        ["qemu-system-riscv64", "--version"],
        capture_output=True,
        text=True,
    )

    return {
        "host": platform.platform(),
        "qemu": version.stdout.splitlines()[0],
        "vcpus": VCPUS,
        "memory": memory,
        "host_cores": host_cores,
    }


def get_logger(name):
    """
    Create the logger used by the harnesses.

    Args:
        name: logger name.

    Returns:
        logging.Logger: console and file logger writing to bench.log next to this module.
    """
    setup = LoggerSetup(name, filename="bench.log", log_dir=os.path.join(HERE, "logs"))

    return setup.get_logger()


def console_log(name):
    """
    Open the file that records the raw guest console of a harness.

    Args:
        name: file name stem.

    Returns:
        file: text file opened for writing in the logs directory next to this module.
    """
    os.makedirs(os.path.join(HERE, "logs"), exist_ok=True)

    return open(os.path.join(HERE, "logs", name + "_console.log"), "w")


def stats(values, digits=1):
    """
    Format values as mean +- standard deviation.

    Args:
        values: list of numbers.
        digits: decimals to print.

    Returns:
        str: the formatted value.
    """
    deviation = statistics.stdev(values) if len(values) > 1 else 0.0

    return "%.*f +- %.*f" % (digits, statistics.mean(values), digits, deviation)


def build_parser(description, prefix):
    """
    Create the argument parser with the options both harnesses accept.

    Args:
        description: help text of the program.
        prefix: file name prefix of the default result file.

    Returns:
        argparse.ArgumentParser: parser with --host-cores and --out.
    """
    parser = argparse.ArgumentParser(
        description=description,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--host-cores",
        default=None,
        help="taskset core list for QEMU, for example 8-15",
    )
    parser.add_argument(
        "--out",
        default=os.path.join(
            HERE, "results", "%s_%s.csv" % (prefix, time.strftime("%Y%m%d_%H%M%S"))
        ),
        help="result CSV file",
    )

    return parser


class Results:
    """
    Writes result rows to a CSV file whose first line describes the host.
    """

    def __init__(self, path, meta):
        """
        Args:
            path: CSV file to create.
            meta: dict written as a comment on the first line.
        """
        self.path = path
        self.meta = meta

    def __enter__(self):
        self.file = open(self.path, "w", newline="")
        self.file.write("# " + json.dumps(self.meta) + "\n")
        self.writer = csv.DictWriter(self.file, FIELDS)
        self.writer.writeheader()

        return self

    def __exit__(self, *exc):
        self.file.close()

    def write(self, row):
        """
        Append one row and flush it to disk.

        Args:
            row: dict with a value for every column of FIELDS.
        """
        self.writer.writerow(row)
        self.file.flush()


class Progress:
    """
    Logs every recorded run and the statistics of each finished group of repetitions.
    """

    def __init__(self, log, spec):
        """
        Args:
            log: logger that receives the messages.
            spec: test definitions from load_spec.
        """
        self.log = log
        self.reps = spec["reps"]
        self.total = len(spec["tests"]) * len(spec["threads"]) * spec["reps"]
        self.done = 0
        self.groups = defaultdict(list)

    def record(self, row):
        """
        Log one finished run, and the group statistics when it completes its group.

        Args:
            row: result row of the run.
        """
        group = self.groups[(row["test"], row["threads"])]
        group.append(row)
        self.done += 1
        label = "[%s] %s threads=%d" % (row["sched"], row["test"], row["threads"])

        self.log.info(
            "%s rep=%d/%d (%d/%d) rate=%.2f ttft=%s ms e2e=%s ms",
            label,
            row["rep"],
            self.reps,
            self.done,
            self.total,
            row["rate_pos_per_s"],
            row["ttft_ms"],
            row["e2e_ms"],
        )

        if len(group) == self.reps:
            rates = [r["rate_pos_per_s"] for r in group]
            ttfts = [r["ttft_ms"] for r in group if r["ttft_ms"] is not None]
            self.log.info("%s: rate %s, ttft %s ms", label, stats(rates), stats(ttfts, 0))
