#!/bin/bash
#
# benchmark.sh - Collect timing data for the serial / POSIX Threads / OpenMP prime search.
#
# Writes results.csv, one row per individual run:
#   experiment,program,n,threads,rep,time_seconds,primes_found
#
# experiment = "vary_n"       -> feeds graphs 1, 2, 5, 6, 7
#            = "vary_threads" -> feeds graphs 3, 4, 8
#
# Build first:
#   gcc -O2 -Wall -o task1 task1.c -lm
#   gcc -O2 -Wall -o task2 task2.c -pthread -lm
#   gcc -O2 -Wall -fopenmp -o task3 task3.c -lm
#
# Usage:
#   ./benchmark.sh
#
set -u

OUT="results.csv"
REPS=3                  # runs per configuration; take the median/min later
FIXED_N=10000000        # n held constant while sweeping the thread count
FIXED_THREADS=8         # thread count held constant while sweeping n

# 30 values of n, roughly log-spaced up to 5*10^7. The spec asks for at least
# 30 different n and recommends going past 10^7. Raise the tail if you have
# the memory for it: task2/task3 allocate 8n bytes twice, so n = 10^8 needs
# about 1.6 GB. Check with "free -h" before extending.
N_VALUES=(
    10000    15000    22000    33000    47000    68000
    100000   150000   220000   330000   470000   680000
    1000000  1500000  2200000  3300000  4700000  6800000
    10000000 12000000 15000000 18000000 22000000 26000000
    30000000 34000000 38000000 42000000 46000000 50000000
)

# 1 up to the logical CPU count (8 here), then past it to show what
# oversubscription does - the spec explicitly asks about this.
THREAD_VALUES=(1 2 3 4 5 6 7 8 10 12 16 24 32)

# --- sanity checks -----------------------------------------------------
for prog in task1 task2 task3; do
    if [ ! -x "./$prog" ]; then
        echo "Error: ./$prog is missing or not executable. Build it first." >&2
        exit 1
    fi
done

echo "experiment,program,n,threads,rep,time_seconds,primes_found" > "$OUT"

# record <experiment> <program> <n> <threads> <rep>
record() {
    local experiment=$1 program=$2 n=$3 threads=$4 rep=$5
    local output time primes

    if [ "$program" = "task1" ]; then
        output=$(./task1 "$n")
    else
        output=$("./$program" "$n" "$threads")
    fi

    time=$(printf '%s\n' "$output" | sed -n 's/.*time taken = \([0-9.]*\) seconds.*/\1/p')
    primes=$(printf '%s\n' "$output" | sed -n 's/.*primes found = \([0-9]*\).*/\1/p')

    if [ -z "$time" ]; then
        echo "  WARNING: could not parse $program n=$n threads=$threads" >&2
        return
    fi

    echo "$experiment,$program,$n,$threads,$rep,$time,$primes" >> "$OUT"
    printf '  %-5s n=%-9s T=%-3s rep=%s  %8ss  (%s primes)\n' \
        "$program" "$n" "$threads" "$rep" "$time" "$primes" >&2
}

echo "=== Experiment A: sweeping n (threads = $FIXED_THREADS) ===" >&2
for n in "${N_VALUES[@]}"; do
    for rep in $(seq 1 "$REPS"); do
        record vary_n task1 "$n" 1                "$rep"
        record vary_n task2 "$n" "$FIXED_THREADS" "$rep"
        record vary_n task3 "$n" "$FIXED_THREADS" "$rep"
    done
done

echo "=== Experiment B: sweeping thread count (n = $FIXED_N) ===" >&2
for rep in $(seq 1 "$REPS"); do
    record vary_threads task1 "$FIXED_N" 1 "$rep"
done
for t in "${THREAD_VALUES[@]}"; do
    for rep in $(seq 1 "$REPS"); do
        record vary_threads task2 "$FIXED_N" "$t" "$rep"
        record vary_threads task3 "$FIXED_N" "$t" "$rep"
    done
done

echo "" >&2
echo "Done. Raw data in $OUT" >&2

# --- correctness cross-check -------------------------------------------
# Every program, at a given n, must report the same number of primes.
echo "" >&2
echo "Correctness check:" >&2
awk -F, 'NR > 1 { seen[$3 "," $7] = 1 }
END {
    for (k in seen) { split(k, a, ","); distinct[a[1]]++ }
    bad = 0
    for (n in distinct)
        if (distinct[n] != 1) { print "  MISMATCH at n=" n; bad = 1 }
    if (!bad) print "  OK - all three programs agree on the prime count for every n."
}' "$OUT" >&2
