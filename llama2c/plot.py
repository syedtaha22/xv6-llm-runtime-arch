# plot.py
import pandas as pd
import matplotlib.pyplot as plt
import sys

# Accept CSV file as command line argument, default to benchmark_results.csv
csv_file = sys.argv[1] if len(sys.argv) > 1 else "benchmark_results.csv"

# Load benchmark results
df = pd.read_csv(csv_file)

# Plot execution time (task-clock) for each model
plt.figure(figsize=(8,5))
plt.bar(df['model'], df['real_time_user_cpu_system_cpu'], color='skyblue')
plt.ylabel('Execution time (ms)')
plt.xlabel('Model')
plt.title('LLM Benchmark - Execution Time')
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig('benchmark_plot.png')
plt.show()
