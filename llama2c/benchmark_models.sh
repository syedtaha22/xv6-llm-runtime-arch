#!/bin/bash
# ============================================================================== 
# Script: benchmark_models.sh
# Description: Benchmarks multiple LLM models using perf, collects statistics,
#              and plots results using an external Python script.
# Author: Syed Taha
# Date: October 26, 2025
#
# Usage:
#   ./benchmark_models.sh [OPTION]
#
# Options:
#   -c                   Clean previous benchmark outputs (CSV, plots)
#   -h                   Display this help message and exit
#
# Assumptions:
#   - The models directory contains .bin model files.
#   - The C program ./run is compiled and executable.
#   - Python 3 and matplotlib/pandas are installed.
# ==============================================================================

# ================= Configuration Variables =================
MODEL_DIR="./models"
OUTPUT_CSV="benchmark_results.csv"
NUM_TOKENS=256
PYTHON_PLOTTER_SCRIPT="plot.py"
PLOTS_DIR="plots"
# ============================================================

show_help() {
    echo "Usage: $0 [OPTION]"
    echo
    echo "Options:"
    echo "  -c       Clean benchmark outputs (CSV, plots)"
    echo "  -h       Show this help message"
    echo
    echo "This script benchmarks LLM models in $MODEL_DIR using perf and plots results."
}

clean_all() {
    echo "[*] Cleaning previous benchmark outputs..."
    rm -f "$OUTPUT_CSV"
    rm -rf "$PLOTS_DIR"
    echo "[+] Clean complete."
}

# Parse options
while getopts "ch" opt; do
    case $opt in
        c)
            clean_all
            exit 0
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

# Validate models directory
if [[ ! -d "$MODEL_DIR" ]]; then
    echo "Error: Models directory '$MODEL_DIR' not found."
    exit 1
fi

# Create plots directory
mkdir -p "$PLOTS_DIR"

# Write CSV header
echo "model,real_time_user_cpu_system_cpu,instructions,cycles,cache_references,cache_misses" > "$OUTPUT_CSV"

# Loop through model files
for model in "$MODEL_DIR"/*.bin; do
    model_name=$(basename "$model")
    echo "[*] Benchmarking $model_name..."

    # Run model with perf stat and capture CSV-like output
    perf_output=$(perf stat -x, -e task-clock,cycles,instructions,cache-references,cache-misses ./run "$model" -n $NUM_TOKENS 2>&1 >/dev/null)

    # Extract the first line starting with a number (CSV)
    perf_line=$(echo "$perf_output" | grep -E '^[0-9]')

    if [[ -z "$perf_line" ]]; then
        echo "Warning: No perf data captured for $model_name"
        perf_line="0,0,0,0,0"
    fi

    # Append results to CSV
    echo "$model_name,$perf_line" >> "$OUTPUT_CSV"
done

echo "[+] Benchmark complete. Results saved to $OUTPUT_CSV"

# Generate plot using Python
if [[ ! -f "$PYTHON_PLOTTER_SCRIPT" ]]; then
    echo "Error: Python plot script '$PYTHON_PLOTTER_SCRIPT' not found."
    exit 1
fi

python3 "$PYTHON_PLOTTER_SCRIPT" "$OUTPUT_CSV" "$PLOTS_DIR"
if [[ $? -ne 0 ]]; then
    echo "Error: Python plotting failed."
    exit 1
fi

echo "[SUCCESS] Benchmarking and plotting completed. Plots saved in $PLOTS_DIR."
