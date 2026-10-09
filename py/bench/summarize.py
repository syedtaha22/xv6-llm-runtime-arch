"""
Summarize benchmark CSV files as Markdown tables.

Prints, for every test, the mean and standard deviation over repetitions of the forward-pass
rate, the speedup over one thread, the time to first token, and the xv6 to Linux rate ratio,
followed by the scheduler comparison.
"""

import argparse
import csv
import statistics
from collections import defaultdict

NUMERIC_COLUMNS = [
    "rate_pos_per_s",
    "ttft_ms",
    "e2e_ms",
    "peak_ram_mb",
    "sampled_tokens",
    "prompt_tokens",
]
SCHEDULERS = ["rr", "priority", "mlfq"]


def read_rows(path):
    """
    Read one result file.

    Args:
        path: CSV file written by a harness.

    Returns:
        list: one dict per run with numeric columns converted to float.
    """
    with open(path) as result_file:
        lines = [line for line in result_file if not line.startswith("#")]

    rows = []

    for row in csv.DictReader(lines):
        for column in NUMERIC_COLUMNS:
            row[column] = float(row[column]) if row[column] else None

        row["threads"] = int(row["threads"])
        rows.append(row)

    return rows


def group(rows, column):
    """
    Group a numeric column by (scheduler, test, thread count).

    Args:
        rows: rows from read_rows.
        column: name of the numeric column.

    Returns:
        dict: (sched, test, threads) mapped to the list of values.
    """
    grouped = defaultdict(list)

    for row in rows:
        if row[column] is not None:
            grouped[(row["sched"], row["test"], row["threads"])].append(row[column])

    return grouped


def mean(values):
    """
    Mean of a list, or None for an empty list.

    Args:
        values: list of floats, or None.

    Returns:
        float: the mean, or None.
    """
    return statistics.mean(values) if values else None


def spread(values, digits=1):
    """
    Format a list of values as mean +- standard deviation.

    Args:
        values: list of floats, or None.
        digits: decimals to print.

    Returns:
        str: the formatted value, or "n/a" for no values.
    """
    if not values:
        return "n/a"

    deviation = statistics.stdev(values) if len(values) > 1 else 0.0

    return "%.*f +- %.*f" % (digits, statistics.mean(values), digits, deviation)


def ratio(numerator, denominator, template):
    """
    Format numerator over denominator, or "n/a" when either is missing.

    Args:
        numerator: float or None.
        denominator: float or None.
        template: format string for the quotient.

    Returns:
        str: the formatted quotient.
    """
    if not numerator or not denominator:
        return "n/a"

    return template % (numerator / denominator)


def print_table(corner, columns, rows):
    """
    Print a Markdown table.

    Args:
        corner: header of the first column.
        columns: headers of the remaining columns.
        rows: list of (label, cells) pairs.
    """
    print("| " + " | ".join([corner] + columns) + " |")
    print("|---|" + "---|" * len(columns))

    for label, cells in rows:
        print("| " + " | ".join([label] + cells) + " |")


def print_test(test, rows, rate, ttft, systems, threads):
    """
    Print the tables of one test.

    Args:
        test: test name.
        rows: all result rows.
        rate: rates grouped by group().
        ttft: first-token times grouped by group().
        systems: scheduler and baseline names to include.
        threads: thread counts to include.
    """
    sample = next(row for row in rows if row["test"] == test)
    columns = ["%d threads" % count for count in threads]

    print(
        "### %s (prompt %d tokens, %d sampled tokens)\n"
        % (test, sample["prompt_tokens"], sample["sampled_tokens"])
    )

    print("Forward passes per second\n")
    print_table(
        "System",
        columns,
        [(s, [spread(rate.get((s, test, t))) for t in threads]) for s in systems],
    )

    print("\nSpeedup over one thread\n")
    print_table(
        "System",
        columns,
        [
            (
                s,
                [
                    ratio(mean(rate.get((s, test, t))), mean(rate.get((s, test, 1))), "%.2fx")
                    for t in threads
                ],
            )
            for s in systems
        ],
    )

    print("\nTime to first token (ms)\n")
    print_table(
        "System",
        columns,
        [(s, [spread(ttft.get((s, test, t)), 0) for t in threads]) for s in systems],
    )
    print()


def print_baseline_ratio(tests, rate, threads):
    """
    Print the xv6 round robin rate as a fraction of the Linux rate.

    Args:
        tests: test names.
        rate: rates grouped by group().
        threads: thread counts to include.
    """
    print("### xv6 (rr) rate as a fraction of Linux rate\n")
    print_table(
        "Test",
        ["%d threads" % count for count in threads],
        [
            (
                test,
                [
                    ratio(
                        mean(rate.get(("rr", test, t))), mean(rate.get(("linux", test, t))), "%.2f"
                    )
                    for t in threads
                ],
            )
            for test in tests
        ],
    )
    print()


def print_scheduler_comparison(tests, rate, schedulers):
    """
    Print the forward-pass rate of each scheduler at 3 threads.

    Args:
        tests: test names.
        rate: rates grouped by group().
        schedulers: scheduler names to include.
    """
    print("### Scheduler comparison at 3 threads (forward passes per second)\n")
    print_table(
        "Test",
        schedulers,
        [(test, [spread(rate.get((s, test, 3))) for s in schedulers]) for test in tests],
    )


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("files", nargs="+")
    args = parser.parse_args()

    rows = [row for path in args.files for row in read_rows(path)]
    rate = group(rows, "rate_pos_per_s")
    ttft = group(rows, "ttft_ms")
    tests = sorted({row["test"] for row in rows})
    threads = sorted({row["threads"] for row in rows})
    systems = sorted({row["sched"] for row in rows})
    schedulers = [s for s in SCHEDULERS if s in systems]

    print("Mean +- standard deviation over %d runs per cell.\n" % len(next(iter(rate.values()))))

    for test in tests:
        print_test(test, rows, rate, ttft, systems, threads)

    if "linux" in systems and "rr" in systems:
        print_baseline_ratio(tests, rate, threads)

    if len(schedulers) > 1 and 3 in threads:
        print_scheduler_comparison(tests, rate, schedulers)


if __name__ == "__main__":
    main()
