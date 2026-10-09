# Benchmarks

This directory holds the benchmark protocol, the harnesses and the recorded results for the
xv6 LLM runtime. Two systems run the same four tests on the same QEMU RISC-V virt machine
(8 vCPUs):

- **xv6**: the kernel and `llama` program of this repository, with the matmul thread pool.
- **linux**: a RISC-V Linux guest running llama2.c with OpenMP, built from
  [run.c](linux-baseline/run.c), a copy of the upstream
  [run.c](https://github.com/karpathy/llama2.c/blob/350e04fe35433e6d2941dce5a1f53308f87058eb/run.c)
  with timing output added that prints the same metrics.

Comparing the two systems on the same emulator separates the cost of the xv6 port from the cost
of QEMU emulation.

## Tests

The test definitions are in [tests.json](tests.json). The model is stories15M, the temperature is
0.0 and the seed is 123, so every run produces the same text.

| Test | Prompt ([tests.json](tests.json)) | Prompt tokens | `-n` (total positions) |
|---|---|---|---|
| T1 | `short` | 5 | 50 |
| T2 | `short` | 5 | 100 |
| T3 | `long` | 50 | 50 |
| T4 | `long` | 50 | 100 |

`-n` counts every position, prompt included, so a run samples `n - (prompt tokens - 1)` new tokens.
T3 samples one token and therefore measures almost only prompt prefill.

## Metrics

- **Rate**: forward passes per second after the first pass, `(pos - 1) / (end - start)`. Prompt
  positions count, so the value is a forward-pass rate and not an output-token rate.
- **TTFT**: milliseconds from the start of `generate()`, prompt prefill included, to the first
  sampled token. It is `n/a` when no token is sampled. llama2.c evaluates the prompt one token at
  a time, so TTFT grows with prompt length.
- **E2E**: milliseconds from program start to program end, including weight attachment and setup.
- **Peak RAM**: peak memory in use, reported by xv6 only.

## Protocol

- Each configuration runs 5 times. Tables report the mean and the standard deviation.
- QEMU runs pinned to host cores with `taskset`, the same cores for both systems.
- The thread counts are 1, 3, 5 and 7. The schedulers are round robin (`rr`), `priority` and
  `mlfq`.
- One warm-up run per thread count follows the first (weight-fetching) run, and none of them is
  recorded.
- The Linux guest has 512 MB of memory because its initramfs holds the 60 MB model. The xv6 guest
  has 256 MB.
- Both systems are compiled with `-O`.

## Known Issues

- The `priority` scheduler stalls while the weights are fetched or immediately after, so no
  `priority` results exist.
- The `mlfq` scheduler runs at 3.5 forward passes per second with 1 thread and 1.7 with 3 threads,
  and a run with 5 threads does not complete. [xv6_mlfq.csv](results/xv6_mlfq.csv) holds the 10
  rows recorded before the hang.
- The harness stops at the first run that produces no report.

## Running

Requirements:

- `qemu-system-riscv64`
- A riscv64 GCC toolchain for xv6 (`riscv64-unknown-elf-`)
- A riscv64 GCC toolchain for the Linux baseline (`riscv64-linux-gnu-`), with the riscv64 `libgomp`
  and `libc` shared libraries
- `curl`, `cpio` and `gzip`

```bash
python3 -m venv py/.venv
source py/.venv/bin/activate
pip install -r py/requirements.txt

# Terminal 1: weight server (xv6 runs only)
python py/server/server.py --port 9999

# Terminal 2
python py/bench/xv6_bench.py --sched rr --host-cores 8-15 --out py/bench/results/xv6.csv
py/bench/linux-baseline/build.sh
python py/bench/linux_bench.py --host-cores 8-15 --out py/bench/results/linux.csv
python py/bench/summarize.py \
    py/bench/results/xv6.csv \
    py/bench/results/linux.csv \
    >py/bench/results/RESULTS.md
```

## Files

| File | Purpose |
|---|---|
| [tests.json](tests.json) | Prompts, tests, thread counts, repetitions, seed |
| [xv6_bench.py](xv6_bench.py) | Builds each scheduler, boots xv6, runs the tests |
| [linux_bench.py](linux_bench.py) | Boots the Linux guest and collects its runs |
| [common.py](common.py) | Result columns, CSV writer, progress logging, host metadata |
| [summarize.py](summarize.py) | Markdown tables from the CSV files |
| [results](results) | CSV files and [RESULTS.md](results/RESULTS.md) |
| [build.sh](linux-baseline/build.sh) | Builds the Linux guest image |
| [run.c](linux-baseline/run.c) | llama2.c run.c with timing output added |
| [extract_kernel.py](linux-baseline/extract_kernel.py) | Extracts the raw kernel Image |
| [write_run_list.py](linux-baseline/write_run_list.py) | Writes the guest's list of runs |
| [init.c](linux-baseline/init.c) | Guest init program that executes the runs |
