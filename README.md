# Optimizing OS Runtime for Large Language Models (LLMs)

This project is an experimental starting point for exploring how an operating system can be modified to handle the unique demands of **Large Language Model (LLM) inference**. The focus is on understanding and improving system-level execution, memory, and scheduling to better support these workloads.

***

## Table of Contents

- [Introduction and Overview](#introduction--overview)
- [What is Implemented](#what-is-implemented)
- [Requirements](#requirements)
- [Build and Run](#build-and-run)
- [Performance](#performance)
- [Known Issues](#known-issues)
- [Repository Layout](#repository-layout)
- [References / Acknowledgements](#references--acknowledgements)

---

## Introduction / Overview

Running LLMs efficiently requires careful attention to how the operating system manages processes, memory, and CPU resources. These models involve **large-scale computations** and **high memory usage**, which can easily become bottlenecks if the system is not optimized at the kernel level.

The aim of this project is to explore modifications to the OS runtime environment to support LLM workloads. This includes investigating **scheduling strategies**, **memory management improvements**, and **kernel-level structures** that affect execution efficiency.

### Key Focus Areas

1. **Scheduling:** Adjusting how the operating system prioritizes and sequences LLM-related processes to reduce delays and improve throughput.  
2. **Memory Management:** Handling the large memory footprint of model weights and activations efficiently.  
3. **Runtime Architecture:** Structuring the OS runtime so that computational and memory resources are utilized effectively, forming a solid foundation for LLM execution.

### Current Status

The runtime runs end to end. xv6 boots with floating-point support, fetches the model weights over UDP, caches them in shared memory, and generates text with a multi-threaded llama2.c port. The scheduling and memory-management work described above is implemented and benchmarked; the results are in the Performance section. The priority and MLFQ schedulers have open issues, listed under Known Issues.

---

## What is Implemented

| Area | Contents |
|---|---|
| Floating point | FPU enabled at boot, f0 to f31 and fcsr saved in the trapframe and across context switches |
| User libraries | `xstdlib`, `xstrlib` and `xmath` (polynomial `exp`, `sin`, `cos`, `tanh`, `log`, `pow`, `sqrt`) with tests |
| Networking | E1000 driver, PCI enumeration, UDP/IP, `bind`, `unbind`, `send`, `recv` system calls |
| Weight transfer | LLM-RFTP, a UDP file-transfer protocol with chunk ranges, retransmission and SHA-256 verification (client in [ftpclient.c](xv6-riscv/user/ftpclient.c), server in [server](py/server)) |
| Persistence | System V style shared memory (`shmget`, `shmat`, `shmdt`, `shmctl`) used as a weight cache that outlives the process |
| Inference | [llama.c](xv6-riscv/user/llama.c), the llama2.c generation loop adapted to xv6, with a built-in profiler ([perf.c](xv6-riscv/user/perf.c)) |
| Threads | `thread_create`, `thread_join`, `thread_exit`, and a thread pool that parallelises matmul and attention |
| Schedulers | Round robin (default), priority ([sched_priority.c](xv6-riscv/kernel/sched_priority.c)) and MLFQ ([sched_mlfq.c](xv6-riscv/kernel/sched_mlfq.c)), selected at build time |

## Requirements

- `qemu-system-riscv64` 7.2 or newer
- A riscv64 GCC toolchain (`riscv64-unknown-elf-`, `riscv64-elf-` or `riscv64-linux-gnu-`)
- Python 3 with the packages in [requirements.txt](py/requirements.txt), installed into a virtual environment at `py/.venv`

## Build and Run

Start the weight server. It serves [stories15M.bin](py/server/models/stories15M.bin) and [tokenizer.bin](py/server/models/tokenizer.bin)
over UDP on port 9999.

```bash
python3 -m venv py/.venv && source py/.venv/bin/activate
pip install -r py/requirements.txt

python py/server/server.py --port 9999
```

Build and boot xv6 in a second terminal.

```bash
cd xv6-riscv
make qemu
```

Run the model at the xv6 shell. The first run fetches the weights and caches them in shared
memory, so later runs start without a transfer.

```
$ llama -x 3 -t 0.0 -s 123 -n 100 -i "Once upon a time"
```

| Flag | Meaning | Default |
|---|---|---|
| `-i` | Prompt | empty |
| `-n` | Total positions to evaluate, prompt included | 256 |
| `-t` | Temperature, 0.0 is greedy | 1.0 |
| `-p` | Top-p for nucleus sampling | 0.9 |
| `-s` | Random seed | time |
| `-x` | Matmul worker threads | 3 |
| `-m` | `generate` or `chat` | `generate` |
| `-y` | System prompt in chat mode | none |

Scheduler variants build with `make SCHED_FLAG=PRIORITY qemu` or `make SCHED_FLAG=MLFQ qemu`.
Userland tests run with `runtests` at the xv6 shell.

## Performance

The numbers below are for stories15M (60 MB) on a QEMU virt machine with 8 vCPUs, QEMU pinned to
8 host cores, `-O` builds, 5 runs per cell. The Linux baseline is a RISC-V Linux guest running
llama2.c with OpenMP on the same emulator, so it separates the cost of the xv6 port from the cost
of emulation. Rate is forward passes per second (prompt positions included). Test T2 uses a
5-token prompt and 96 sampled tokens.

| System | 1 thread | 3 threads | 5 threads | 7 threads |
|---|---|---|---|---|
| xv6, round robin | 7.5 +- 0.0 | 12.0 +- 0.2 | 11.2 +- 0.0 | 10.7 +- 0.0 |
| Linux guest | 7.1 +- 0.1 | 18.2 +- 0.2 | 16.1 +- 0.2 | 19.9 +- 0.3 |
| xv6 speedup over 1 thread | 1.00x | 1.60x | 1.50x | 1.43x |
| Linux speedup over 1 thread | 1.00x | 2.57x | 2.28x | 2.81x |

- Single-thread throughput of xv6 matches or slightly exceeds the Linux guest (ratio 1.06 to 1.11 across the four tests).
- With 3 or more threads xv6 reaches 0.53 to 0.71 of the Linux rate. The Linux guest keeps scaling
  to 7 threads while xv6 stays flat or declines from 3 threads, so the limit is in the xv6 thread pool or
  synchronisation and not in the emulator.
- Matmul dominates the single-thread profile (89 to 92% of the run time).
- The first execution after boot spends about 28 s fetching the weights over UDP. Later executions
  attach the cached segment in under 100 ms.
- For the T1 prompt at temperature 0, the output text matches upstream llama2.c.

Full tables (all four tests, speedups, time to first token, xv6 to Linux ratios) are in
[RESULTS.md](py/bench/results/RESULTS.md). The protocol and the metric definitions are in
[README.md](py/bench/README.md).

## Known Issues

- The priority scheduler stalls while the weights are fetched or immediately after, so `llama`
  does not complete under `SCHED_FLAG=PRIORITY`.
- Under MLFQ, throughput is 3.5 forward passes per second with 1 thread and 1.7 with 3 threads,
  and a run with 5 threads does not complete.
- Only the round robin scheduler has a complete benchmark.
- Results come from QEMU emulation and are not hardware measurements.
- Only stories15M is run end to end on xv6.

## Repository Layout

```
.
├── py
│   ├── bench
│   │   ├── linux-baseline
│   │   │   ├── build.sh              # builds kernel image, llama2.c binary, init and initramfs
│   │   │   ├── extract_kernel.py     # unpacks the raw kernel Image
│   │   │   ├── init.c                # guest PID 1: runs the test list, powers off
│   │   │   ├── run.c                 # llama2.c run.c with timing output added
│   │   │   └── write_run_list.py     # writes the guest's list of runs
│   │   ├── results/                  # recorded runs
│   │   ├── common.py
│   │   ├── linux_bench.py            # boots the Linux guest, writes CSV
│   │   ├── README.md                 # protocol and metrics
│   │   ├── summarize.py
│   │   ├── tests.json                # prompts, tests, threads, repetitions, seed
│   │   └── xv6_bench.py              # builds each scheduler, boots xv6, writes CSV
│   ├── server
│   │   ├── models/                   # model files
│   │   ├── client.py                 # test client for the server
│   │   ├── README.md                 # server usage
│   │   └── server.py                 # UDP server for LLM-RFTP
│   ├── custom_logger.py
│   └── requirements.txt              
├── xv6-riscv
│   ├── conf/
│   ├── kernel/                       # FPU, E1000 and UDP, shared memory, threads, schedulers
│   ├── mkfs/
│   ├── user/                         # llama, perf, xstdlib, xstrlib, xmath, tests
│   ├── LICENSE
│   ├── Makefile
│   └── README
├── CONTRIBUTING.md                   # branching, commit and code conventions
└── README.md                         # this file
```

---

## References / Acknowledgements

- **LLama2.c:** This project uses [llama2.c](https://github.com/karpathy/llama2.c) as the current inference engine for testing and experimentation.  
- **xv6-riscv:** the MIT teaching operating system this project extends.
- **Book:** [From Boot to Inference: Building an LLM Runtime in xv6](https://github.com/syedtaha22/xv6-llm-book) documents the design chapter by chapter.
- **Authors:** Syed Taha, Hamna Sajid, Hadiya Muneeb and Zarmeen Rahman.
