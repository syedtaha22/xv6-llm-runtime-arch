"""
Benchmark harness for the xv6 LLM runtime.

Boots xv6 under QEMU for each scheduler, warms the shared-memory weight cache, runs every
(test, thread count, repetition) combination and writes one CSV row per run. Metrics are parsed
from the report that the guest program prints.
"""

import os
import re
import shutil
import subprocess
import sys
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

XV6_DIR = os.path.join(HERE, "..", "..", "xv6-riscv")
BUILD_DIR = os.path.join(HERE, "build")
SCHED_FLAGS = {"rr": None, "priority": "PRIORITY", "mlfq": "MLFQ"}
MEMORY = "256M"

SHELL_PROMPT = r"\$ "
REPORT_END = r"SYSTEM PERFORMANCE METRICS[\s\S]*?={20,}\r?\n[\s\S]*?\$ "

REPORT_PATTERNS = {
    "prompt_tokens": (r"Prompt\s+(\d+) tokens", int),
    "generated": (r"Generated\s+(\d+) tokens", int),
    "rate_pos_per_s": (r"Rate\s+([0-9.]+) tokens/sec", float),
    "inference_ms": (r"Inference\s+(\d+) ms", int),
    "e2e_ms": (r"Total \(E2EL\)\s+(\d+) ms", int),
    "peak_ram_mb": (r"Peak\s+([0-9.]+) MB", float),
}
TTFT_PATTERN = r"TTFT\s+(?:n/a|(-?[0-9.]+) ms)"


def build(sched, out_dir, log):
    """
    Build the kernel and file system image for a scheduler.

    Compiler and linker messages are logged as warnings. A failed build is logged and raised.

    Args:
        sched: key of SCHED_FLAGS.
        out_dir: directory that receives copies of kernel and fs.img.
        log: logger for progress and build messages.
    """
    make = ["make", "-j8", "fs.img", "kernel/kernel"]

    if SCHED_FLAGS[sched]:
        make.insert(1, "SCHED_FLAG=" + SCHED_FLAGS[sched])

    for command in (["make", "clean"], make):
        log.info("[%s] %s", sched, " ".join(command))
        result = subprocess.run(command, cwd=XV6_DIR, capture_output=True, text=True)

        for line in result.stderr.splitlines():
            if line.strip():
                log.warning("[%s] build: %s", sched, line)

        if result.returncode != 0:
            log.error(
                "[%s] %s failed with exit code %d", sched, " ".join(command), result.returncode
            )
            raise subprocess.CalledProcessError(result.returncode, command)

    os.makedirs(out_dir, exist_ok=True)
    shutil.copy(os.path.join(XV6_DIR, "kernel", "kernel"), out_dir)
    shutil.copy(os.path.join(XV6_DIR, "fs.img"), out_dir)


def qemu_command(out_dir, host_cores):
    """
    Build the QEMU command line used for xv6.

    Args:
        out_dir: directory holding kernel and fs.img.
        host_cores: taskset core list, or None for no pinning.

    Returns:
        list: argv for the QEMU process.
    """
    disk = "file=%s,if=none,format=raw,id=x0" % os.path.join(out_dir, "fs.img")
    command = [
        "qemu-system-riscv64",
        "-machine",
        "virt",
        "-bios",
        "none",
        "-kernel",
        os.path.join(out_dir, "kernel"),
        "-m",
        MEMORY,
        "-smp",
        str(VCPUS),
        "-nographic",
        "-global",
        "virtio-mmio.force-legacy=false",
        "-drive",
        disk,
        "-device",
        "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0",
        "-netdev",
        "user,id=net0",
        "-device",
        "e1000,netdev=net0,bus=pcie.0",
    ]

    return ["taskset", "-c", host_cores] + command if host_cores else command


def parse_report(text):
    """
    Extract the metrics from the report printed by the guest program.

    Args:
        text: console output of one run.

    Returns:
        dict: one value per metric column, or None when no report is present.
    """
    if "SYSTEM PERFORMANCE METRICS" not in text:
        return None

    values = {}

    for name, (pattern, cast) in REPORT_PATTERNS.items():
        match = re.search(pattern, text)
        values[name] = cast(match.group(1)) if match else None

    ttft = re.search(TTFT_PATTERN, text)
    values["ttft_ms"] = float(ttft.group(1)) if ttft and ttft.group(1) else None

    generated = values.pop("generated")
    values["pos_final"] = None if generated is None else generated + 1
    values["sampled_tokens"] = None

    if values["pos_final"] is not None and values["prompt_tokens"] is not None:
        values["sampled_tokens"] = max(0, values["pos_final"] - (values["prompt_tokens"] - 1))

    return values


def run_llama(child, threads, prompt, steps, seed, timeout=300):
    """
    Run llama once inside the xv6 shell and parse its report.

    Args:
        child: pexpect handle of the running QEMU process.
        threads: matmul worker thread count.
        prompt: prompt text.
        steps: total positions, prompt included.
        seed: sampler seed.
        timeout: seconds to wait for the report.

    Returns:
        dict: metrics from parse_report.
    """
    child.sendline('llama -x %d -t 0.0 -s %d -n %d -i "%s"' % (threads, seed, steps, prompt))
    child.expect(REPORT_END, timeout=timeout)

    return parse_report(child.before + child.after)


def boot(sched, out_dir, host_cores, log):
    """
    Start xv6 under QEMU and wait for the shell.

    Args:
        sched: scheduler key, used for the log and the console file name.
        out_dir: directory holding kernel and fs.img.
        host_cores: taskset core list, or None for no pinning.
        log: logger for progress messages.

    Returns:
        pexpect.spawn: handle of the running QEMU process, at the shell prompt.
    """
    command = qemu_command(out_dir, host_cores)
    log.info("[%s] booting: %s", sched, " ".join(command))
    started = time.time()

    child = pexpect.spawn(command[0], command[1:], encoding="utf-8", timeout=300)
    child.logfile_read = console_log("xv6_" + sched)
    child.expect(SHELL_PROMPT)
    log.info("[%s] shell reached in %.1f s", sched, time.time() - started)

    return child


def warm_up(child, sched, spec, log):
    """
    Run the unrecorded warm-up runs.

    The first run fetches the weights into shared memory. One further run per thread count brings
    every code path to steady state.

    Args:
        child: pexpect handle of the running QEMU process.
        sched: scheduler key, used for the log.
        spec: test definitions from load_spec.
        log: logger for progress messages.
    """
    prompt = spec["prompts"]["short"]

    log.info("[%s] warm-up: fetching weights over UDP", sched)
    started = time.time()
    run_llama(child, 3, prompt, 20, spec["seed"], timeout=600)
    log.info("[%s] weights fetched and first run finished in %.1f s", sched, time.time() - started)

    for threads in spec["threads"]:
        run_llama(child, threads, prompt, 50, spec["seed"])
        log.info("[%s] warm-up threads=%d done", sched, threads)


def run_matrix(child, sched, spec, results, log):
    """
    Run every test, thread count and repetition and record each run.

    Args:
        child: pexpect handle of the running QEMU process.
        sched: scheduler key, written to the result rows.
        spec: test definitions from load_spec.
        results: Results that receives one row per run.
        log: logger for progress messages.
    """
    progress = Progress(log, spec)
    started = time.time()

    for name, test in spec["tests"].items():
        prompt = spec["prompts"][test["prompt"]]

        for threads in spec["threads"]:
            for rep in range(1, spec["reps"] + 1):
                row = run_llama(child, threads, prompt, test["steps"], spec["seed"])

                if row is None:
                    log.error(
                        "[%s] %s threads=%d rep=%d: no report in the console",
                        sched,
                        name,
                        threads,
                        rep,
                    )
                    sys.exit(1)

                row.update(sched=sched, test=name, threads=threads, rep=rep)
                results.write(row)
                progress.record(row)

    log.info("[%s] finished %d runs in %.0f s", sched, progress.total, time.time() - started)


def main():
    parser = build_parser(__doc__, "xv6")
    parser.add_argument("--sched", nargs="+", default=["rr"], choices=SCHED_FLAGS)
    args = parser.parse_args()

    log = get_logger("xv6_bench")
    spec = load_spec()

    with Results(args.out, host_meta(args.host_cores, MEMORY)) as results:
        for sched in args.sched:
            out_dir = os.path.join(BUILD_DIR, sched)

            started = time.time()
            build(sched, out_dir, log)
            log.info("[%s] built in %.0f s", sched, time.time() - started)

            child = boot(sched, out_dir, args.host_cores, log)
            warm_up(child, sched, spec, log)
            run_matrix(child, sched, spec, results, log)
            child.close(force=True)

    log.info("wrote %s", args.out)


if __name__ == "__main__":
    main()
