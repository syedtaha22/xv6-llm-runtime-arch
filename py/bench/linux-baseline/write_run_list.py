"""
Write the list of runs executed by the Linux guest.

Usage: write_run_list.py <tests.json> <output>

Each line holds "<test> <threads> <rep> <steps> <prompt>". A warm-up line comes first so that the
first recorded run does not pay for a cold page cache.
"""

import json
import sys


def main():
    with open(sys.argv[1]) as spec_file:
        spec = json.load(spec_file)

    with open(sys.argv[2], "w") as run_list:
        run_list.write("warmup 3 0 20 %s\n" % spec["prompts"]["short"])

        for name, test in spec["tests"].items():
            prompt = spec["prompts"][test["prompt"]]

            for threads in spec["threads"]:
                for rep in range(1, spec["reps"] + 1):
                    run_list.write("%s %d %d %d %s\n" % (name, threads, rep, test["steps"], prompt))


if __name__ == "__main__":
    main()
