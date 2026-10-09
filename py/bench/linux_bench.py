"""
Benchmark harness for the Linux baseline.

Boots a RISC-V Linux guest under QEMU with the same vCPU count as the xv6 runs. The guest runs
every (test, thread count, repetition) and powers off. One CSV row is written per run.
"""

import os
import re
import time

import pexpect

from common import (
    HERE,
    VCPUS,
    Progress,
    Results,
    build_parser,
    console_log,
    get_logger,
    host_meta,
    load_spec,
)

OUT_DIR = os.path.join(HERE, "linux-baseline", "out")
GUEST_KERNEL = "Ubuntu linux-image-6.8.0-60-generic (riscv64)"
KERNEL_ARGS = "console=ttyS0 rdinit=/init panic=-1 loglevel=3"

# The initramfs holds the 60 MB model, which does not unpack into 256 MB.
MEMORY = "512M"

RUN_LINE = re.compile(r"RUN test=(?P<test>\S+) threads=(?P<threads>\d+) rep=(?P<rep>\d+) steps=\d+")
BENCH_LINE = re.compile(
    r"BENCH prompt=(?P<prompt>\d+) pos=(?P<pos>\d+) sampled=(?P<sampled>\d+) "
    r"ttft_ms=(?P<ttft>-?\d+) rate=(?P<rate>[0-9.]+) window_ms=(?P<window>\d+) "
    r"total_ms=(?P<total>\d+)"
)

EVENTS = [RUN_LINE, BENCH_LINE, "BENCH_DONE", pexpect.EOF, "INIT: ready"]
RUN, BENCH, DONE, EOF, READY = range(len(EVENTS))


def qemu_command(host_cores):
    """
    Build the QEMU command line used for the Linux guest.

    Args:
        host_cores: taskset core list, or None for no pinning.

    Returns:
        list: argv for the QEMU process.
    """
    command = [
        "qemu-system-riscv64",
        "-machine",
        "virt",
        "-m",
        MEMORY,
        "-smp",
        str(VCPUS),
        "-nographic",
        "-no-reboot",
        "-kernel",
        os.path.join(OUT_DIR, "Image"),
        "-initrd",
        os.path.join(OUT_DIR, "initramfs.cpio.gz"),
        "-append",
        KERNEL_ARGS,
    ]

    return ["taskset", "-c", host_cores] + command if host_cores else command


def make_row(run, bench):
    """
    Build a result row from the guest's run and result lines.

    Args:
        run: groups of the RUN line (test, threads, rep).
        bench: groups of the BENCH line.

    Returns:
        dict: row with a value for every column of FIELDS.
    """
    ttft = int(bench["ttft"])

    return {
        "sched": "linux",
        "test": run["test"],
        "threads": int(run["threads"]),
        "rep": int(run["rep"]),
        "prompt_tokens": int(bench["prompt"]),
        "pos_final": int(bench["pos"]),
        "sampled_tokens": int(bench["sampled"]),
        "ttft_ms": None if ttft < 0 else ttft,
        "rate_pos_per_s": float(bench["rate"]),
        "inference_ms": int(bench["window"]),
        "e2e_ms": int(bench["total"]),
        "peak_ram_mb": None,
    }


def main():
    parser = build_parser(__doc__, "linux")
    args = parser.parse_args()

    log = get_logger("linux_bench")
    spec = load_spec()
    progress = Progress(log, spec)

    command = qemu_command(args.host_cores)
    log.info("booting: %s", " ".join(command))
    started = time.time()
    child = pexpect.spawn(command[0], command[1:], encoding="utf-8", timeout=3600)
    child.logfile_read = console_log("linux")

    meta = host_meta(args.host_cores, MEMORY)
    meta["guest_kernel"] = GUEST_KERNEL
    run = None

    with Results(args.out, meta) as results:
        while True:
            event = child.expect(EVENTS)

            if event == READY:
                log.info(
                    "guest booted in %.1f s, running %d recorded runs",
                    time.time() - started,
                    progress.total,
                )
                started = time.time()
            elif event == RUN:
                run = child.match.groupdict()
            elif event == BENCH and run["test"] != "warmup":
                row = make_row(run, child.match.groupdict())
                results.write(row)
                progress.record(row)
            elif event in (DONE, EOF):
                break

    log.info("guest finished %d runs in %.0f s", progress.done, time.time() - started)
    child.close(force=True)
    log.info("wrote %s", args.out)


if __name__ == "__main__":
    main()
