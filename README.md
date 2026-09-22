# Parallel prime search: serial, POSIX Threads and OpenMP

Three C programs that find every prime below *n*, written to compare
shared-memory parallelisation approaches on the same workload:

| Program | Approach | Work distribution |
|---|---|---|
| `task1.c` | Serial baseline | — |
| `task2.c` | POSIX Threads | Cyclic: thread *t* tests `3 + 2t`, `3 + 2t + 2T`, … |
| `task3.c` | OpenMP | Cyclic via `#pragma omp parallel for schedule(static, 1)` |

All three share an identical primality test (trial division up to √k,
odd divisors only), so measured differences come from the parallelisation
alone. Neither parallel version needs a lock: results are written into a
slot indexed by the value being tested, so no two threads touch the same
memory.

Built by Haruto Iriyama as a two-person university project. The follow-up —
the same problem on distributed memory with Open MPI and a hybrid MPI + OpenMP
version — is in [parallel-primes-mpi](https://github.com/Haruto03/parallel-primes-mpi).

## Key findings

Full write-up with all numbers in [RESULTS.md](RESULTS.md).

- **Fixing the algorithm beat parallelising it by three orders of magnitude.**
  Naive trial division to *k − 1* takes 113.8 s at n = 10⁶; stopping at √k
  and skipping even divisors brings the *serial* version to 0.104 s — faster
  than the naive version would be even with perfect 8-way parallel speed-up.
- **Partitioning, not the threading API, was the bottleneck.** Contiguous
  blocks left the thread holding the largest numbers finishing last; switching
  to cyclic partitioning gave +18–23 % speed-up at large n.
- **POSIX Threads and OpenMP perform the same** once both use the same
  partitioning (within ~3 %). The apparent 20 % OpenMP advantage seen earlier
  was entirely the partitioning difference.
- Speed-up tops out around 4.5–5× on 8 logical / 4 physical cores, as SMT
  siblings share execution units.

![Speed-up vs threads](graphs/graph8_runtime_vs_threads_pthread_openmp.png)

## Building and running

```bash
gcc -O2 -Wall -o task1 task1.c -lm
gcc -O2 -Wall -o task2 task2.c -pthread -lm
gcc -O2 -Wall -fopenmp -o task3 task3.c -lm

./task1 10000000          # writes primes_output.txt
./task2 10000000 8        # 8 threads, writes primes_output_parallel.txt
./task3 10000000 8        # writes primes_output_openmp.txt
./verify.sh               # checks all three outputs agree
```

Numbers below 100 print to stdout; larger runs write to a file.

## Reproducing the measurements

```bash
gcc -O2 -Wall -o optbench optbench.c -lm
bash benchmark.sh && bash summarize.sh   # ~350 runs, ~15-20 min -> results.csv, speedup.csv
bash optbench.sh                         # primality-test comparison -> optimisation.csv
python plot.py                           # renders graphs/*.png (needs matplotlib)
```

| File | Purpose |
|---|---|
| `benchmark.sh`, `summarize.sh` | Sweep n (30 values to 5×10⁷) and thread counts (1–32), best-of-3 |
| `optbench.c`, `optbench.sh` | Three primality-test variants behind an identical harness |
| `task3_sched.c` | Experiment: `schedule(runtime)` to compare OpenMP schedules |
| `results.csv`, `results_contiguous.csv` | Raw timings after / before the cyclic-partitioning fix |
| `speedup.csv`, `optimisation.csv` | Aggregated data behind the graphs |
| `graphs/` | The nine graphs |
| `commands.txt` | Hand-run verification commands |

Measurements were taken on an AMD Ryzen 5 7535HS (4 cores / 8 threads)
inside a Linux (Ubuntu) Docker container.
