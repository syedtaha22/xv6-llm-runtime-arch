#!/bin/bash
# ==============================================================================
# Script: benchmark_models.sh
# Description: Benchmarks multiple LLM models using perf, collecting detailed
#              system-level and per-function profiling statistics.
# Author: Syed Taha
# Date: October 26, 2025
#
# Usage:
#   ./benchmark_models.sh [OPTION]
#
# Options:
#   -c                   Clean previous benchmark outputs (CSV, results/)
#   -h                   Display this help message and exit
#   -d                   Enable detailed system-wide profiling (includes libc/kernel)
#
# Notes:
#   - Without -d, only user-space functions from run.c are analyzed.
#   - The 'results/' directory will contain:
#         results/
#           ├── stories15M/
#           │    ├── summary.csv
#           │    ├── perf_stat.txt
#           │    └── perf_report.txt
#           └── stories110M/...
# ==============================================================================

# ================= Configuration Variables =================
MODEL_DIR="./models"
RESULTS_DIR="results"
OUTPUT_CSV="${RESULTS_DIR}/benchmark_summary.csv"
NUM_TOKENS=256
DETAILED=0
# ============================================================

show_help() {
    echo "Usage: $0 [OPTION]"
    echo
    echo "Options:"
    echo "  -c       Clean benchmark outputs (CSV, results)"
    echo "  -d       Enable detailed profiling (includes libc/kernel)"
    echo "  -h       Show this help message"
    echo
    echo "This script benchmarks LLM models in $MODEL_DIR using perf and writes detailed statistics to $RESULTS_DIR."
}

clean_all() {
    echo "[*] Cleaning previous benchmark outputs..."
    rm -rf "$RESULTS_DIR"
    echo "[+] Clean complete."
}

# ---------------- Parse options ----------------
while getopts "cdh" opt; do
    case $opt in
        c)
            clean_all
            exit 0
            ;;
        d)
            DETAILED=1
            ;;
        h)
            show_help
            exit 0
            ;;
        \?)
            echo "Error: Invalid option -$OPTARG" >&2
            show_help
            exit 1
            ;;
    esac
done

shift $((OPTIND -1))

# ---------------- Validate models ----------------
if [[ ! -d "$MODEL_DIR" ]]; then
    echo "Error: Models directory '$MODEL_DIR' not found."
    exit 1
fi

mkdir -p "$RESULTS_DIR"

# Write CSV header
echo "model,task_clock_ms,cycles,instructions,ipc,cache_miss_rate(%)" > "$OUTPUT_CSV"

# ================= Benchmark Loop =================
for model in "$MODEL_DIR"/*.bin; do
    model_name=$(basename "$model" .bin)
    model_result_dir="${RESULTS_DIR}/${model_name}"
    mkdir -p "$model_result_dir"

    echo "[*] Benchmarking ${model_name}..."

    # ---- Perf stat ----
    perf_stat_file="${model_result_dir}/perf_stat.txt"
    perf stat -x, \
        -e task-clock,cycles,instructions,cache-references,cache-misses \
        ./run "$model" -n $NUM_TOKENS 2> "$perf_stat_file" >/dev/null

    # ---- Extract key metrics ----
    task_clock=$(grep ",task-clock," "$perf_stat_file" | cut -d, -f1)
    cycles=$(grep ",cycles," "$perf_stat_file" | cut -d, -f1)
    instructions=$(grep ",instructions," "$perf_stat_file" | cut -d, -f1)
    cache_refs=$(grep ",cache-references," "$perf_stat_file" | cut -d, -f1)
    cache_misses=$(grep ",cache-misses," "$perf_stat_file" | cut -d, -f1)

    # Derived metrics
    ipc=$(awk -v i="$instructions" -v c="$cycles" 'BEGIN { if (c>0) printf "%.3f", i/c; else print "0" }')
    miss_rate=$(awk -v m="$cache_misses" -v r="$cache_refs" 'BEGIN { if (r>0) printf "%.2f", (m/r)*100; else print "0" }')

    # Append summary line
    echo "${model_name}.bin,${task_clock},${cycles},${instructions},${ipc},${miss_rate}" >> "$OUTPUT_CSV"

    # ---- Perf record + report ----
    perf_record_file="${model_result_dir}/perf.data"
    perf_report_file="${model_result_dir}/perf_report.txt"

    echo "[*] Capturing detailed function-level stats..."


    if [[ $DETAILED -eq 1 ]]; then
        # full system-wide report (includes kernel/libc)
        perf record -g -o "$perf_record_file" ./run "$model" -n $NUM_TOKENS >/dev/null 2>&1
        perf report --stdio -i "$perf_record_file" > "$perf_report_file"
    else
        # only capture user-space functions (exclude kernel/hypervisor)
        perf record --all-user -o "$perf_record_file" ./run "$model" -n $NUM_TOKENS >/dev/null 2>&1
        perf report --stdio -i "$perf_record_file" > "$perf_report_file"
    fi

    rm -f "$perf_record_file"

    echo "[+] Finished $model_name (stats in $model_result_dir)"
done

echo
echo "[✓] All benchmarks complete."
echo "[→] Summary CSV: $OUTPUT_CSV"
echo "[→] Detailed reports: $RESULTS_DIR/<model_name>/perf_report.txt"
if [[ $DETAILED -eq 1 ]]; then
    echo "[→] Mode: FULL SYSTEM PROFILING"
else
    echo "[→] Mode: FILTERED (functions from run.c only)"
fi
