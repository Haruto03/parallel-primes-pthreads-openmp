# Experiments and Results

Everything measured, and what it means.

**Test machine:** AMD Ryzen 5 7535HS — **4 physical cores / 8 logical**
(SMT, 2 threads per core), inside a Linux (Ubuntu) Docker container.
All numbers below come from this machine. If we also measure on the other
laptop (Intel i7-12650H, 6 P-cores + 4 E-cores = 10 physical / 16 logical),
that data must be kept separate — mixing two machines in one graph makes the
speed-up numbers meaningless.

---

## 1. The three programs

All three share the identical `is_prime` (trial division to `sqrt(k)`,
skipping even divisors), so any measured difference reflects parallelisation
alone and not a difference in the amount of real work.

| | Partitioning | Result collection |
|---|---|---|
| `task1.c` serial | — | writes into a buffer sized by `estimate_capacity` (prime number theorem, `1.2n/ln n`), grown with `realloc` if it undershoots |
| `task2.c` POSIX Threads | **cyclic**: thread `t` takes `k = 3 + 2t`, strides by `2 × num_threads` | shared `results[]` of size n, `results[k] = k`; sequential compaction pass afterwards |
| `task3.c` OpenMP | **cyclic**: `#pragma omp parallel for schedule(static, 1)` over the odd numbers | same as task2 |

Both parallel versions use the **same** partitioning strategy on purpose, so
graphs 7 and 8 isolate the threading API rather than confounding it with the
work distribution.

Neither parallel version needs a lock: `results[k]` is indexed by the value
being tested, so two threads never write to the same slot.

---

## 2. How the measurements were made

`benchmark.sh` automates ~350 runs and writes `results.csv`:

- **Experiment A** — 30 values of n from 10,000 to 50,000,000, all three
  programs, 8 threads, **3 repetitions each**
- **Experiment B** — n fixed at 10,000,000, thread counts 1–8, 10, 12, 16,
  24, 32, both parallel programs, 3 repetitions each

`summarize.sh` aggregates to `speedup.csv`, taking the **best (minimum)** of
the repetitions.

**Why best-of-N and not the mean:** the parallel runs vary by up to **22%**
run to run (the serial run varies by only 4%) because thread placement is
decided by the OS afresh each time. Using the mean produced visible artefacts
in the speed-up curve; the minimum reports what the code can actually do.

**Correctness:** every run records how many primes it found, and the script
checks that all three programs agree at every n — they do. The full output
files were also compared directly:

```bash
./task1 10000000 && ./task2 10000000 8 && diff primes_output.txt primes_output_parallel.txt
```

---

## 3. Findings

### 3.1 Algorithmic optimisation beats parallelisation by three orders of magnitude

`optbench.c` runs three versions of the primality test behind an identical
main and timing harness, so only `is_prime` differs.

| n | v1 trial division to k−1 | v2 stop at √k | v3 √k + odd only | **v1 / v3** | v2 / v3 |
|---|---|---|---|---|---|
| 10,000 | 0.0170 s | 0.000373 s | 0.000191 s | **89×** | 1.95 |
| 100,000 | 1.395 s | 0.008336 s | 0.004608 s | **303×** | 1.81 |
| 1,000,000 | 113.76 s | 0.211434 s | 0.104392 s | **1090×** | 2.03 |

![Task 1 optimisation](graphs/graph0_task1_optimisation.png)

- **v2 / v3 is a consistent ≈ 2.0×** — skipping even divisors halves the
  division count exactly, as theory predicts.
- **v1 / v3 grows with n** (89× → 1090×) because the naive test costs O(k)
  divisions per prime while the others cost O(√k). On the log-log plot the
  measured slopes are ≈ **1.9** for v1 and ≈ **1.37** for v2/v3.

**The headline comparison:** at n = 10⁶ the naive version takes 113.8 s.
Even parallelised perfectly across 8 threads it would still need ~38 s, while
the optimised *serial* version finishes in 0.104 s — **360× faster than a
perfectly parallelised naive implementation.** Parallelisation is not a
substitute for fixing the algorithm.

Sanity check: `optbench` v3 at n = 10⁶ (0.1044 s) matches `task1` at the same
n (0.1022 s) — two separate programs measuring the same thing agree.

### 3.2 Partitioning scheme, not the API, was the real bottleneck

`task2.c` originally gave each thread one contiguous block of `[2, n)`. Since
`is_prime(k)` gets more expensive as k grows, the thread holding the highest
block finished last and capped the speed-up. Switching to cyclic partitioning:

| n | contiguous | cyclic | gain |
|---|---|---|---|
| 6,800,000 | 3.83 | 4.52 | **+18.1%** |
| 22,000,000 | 3.89 | 4.77 | **+22.8%** |
| 38,000,000 | 4.17 | 5.03 | **+20.7%** |
| 50,000,000 | 4.04 | 4.88 | **+20.8%** |

*(`results_contiguous.csv` is the full "before" dataset — same 30 values of n.)*

At n = 1,000,000 cyclic is 9.3% **slower** — at small n the better cache
locality of contiguous blocks outweighs the load imbalance. The trade-off
reverses as n grows.

### 3.3 POSIX Threads and OpenMP perform identically

Before the fix OpenMP looked ~20% faster. That gap was **entirely** the
partitioning difference (`schedule(static, 1)` is cyclic; task2 was
contiguous). With both on cyclic:

| n | pthread | OpenMP | difference |
|---|---|---|---|
| 10,000,000 | 4.50 | 4.38 | 2.7% |
| 22,000,000 | 4.77 | 5.00 | 4.6% |
| 38,000,000 | 5.03 | 4.97 | 1.2% |
| 50,000,000 | 4.88 | 4.94 | 1.3% |

All within measurement noise.

![Graph 7](graphs/graph7_runtime_vs_n_pthread_openmp.png)

Below n ≈ 500,000 OpenMP is erratic and slower — the libgomp runtime has to
build its thread pool, which is pure overhead for a program that enters one
parallel region and exits. At n = 10,000 OpenMP was ~10× slower than the raw
pthread version.

### 3.4 Speed-up depends on problem size

![Graph 2](graphs/graph2_speedup_vs_n_pthread.png)

- Speed-up is **below 1** for small n — parallelisation costs more than it saves
- Break-even at **n ≈ 68,000** for pthread, **n ≈ 150,000–470,000** for OpenMP
- Peak **5.03×** (pthread, n = 38M) and **5.12×** (OpenMP, n = 42M)

Both peaks **exceed the physical core count of 4** — the extra comes from SMT.

### 3.5 Thread scaling is not smooth, and that is a placement effect

![Graph 4](graphs/graph4_speedup_vs_threads_pthread.png)

| threads | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 10 | 16 | 32 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| speed-up | 0.97 | 1.65 | 1.74 | 3.09 | 3.03 | 2.97 | 3.88 | 4.72 | **4.84** | 4.76 | 4.71 |

The curve is a staircase: it jumps at 3→4 and 6→7 but stalls at 4→6. The OS
does not guarantee that threads land on distinct physical cores. Verified
directly with `taskset` at 4 threads:

```bash
taskset -c 0,1,2,3 ./task2 10000000 4   # 2 physical cores (SMT siblings) -> 1.210 s
taskset -c 0,2,4,6 ./task2 10000000 4   # 4 physical cores               -> 1.069 s
```

**13.3% difference for the same 4 threads.** Sibling map on this CPU:
logical 0–1 = core 0, 2–3 = core 1, 4–5 = core 2, 6–7 = core 3
(`/sys/devices/system/cpu/cpu0/topology/thread_siblings_list`).

At 8 threads and above every logical CPU is in use, so placement stops
mattering and the curve settles.

**Oversubscription is benign:** 16, 24 and 32 threads all hold ~4.7×. The
work is compute-bound, so the extra context switching costs little.

**1 thread is slightly slower than the serial version** (0.97×). The parallel
versions carry the `results[]` array and its O(n) compaction pass regardless
of thread count; at 1 thread that overhead shows up with nothing to offset it.

### 3.6 OpenMP scheduling: only cyclic-vs-contiguous matters

Measured with `task3_sched.c` (a copy of task3.c using `schedule(runtime)`),
5 runs each at n = 10⁷, 8 threads:

| schedule | fastest | **median** | slowest | vs `static,1` |
|---|---|---|---|---|
| `static` (default chunk) | 0.6445 | **0.6475** | 0.7202 | **11.5% slower** |
| `static, 1` | 0.5621 | **0.5809** | 0.6400 | — |
| `dynamic, 1000` | 0.5502 | **0.5708** | 0.6187 | 1.7% faster |
| `guided` | 0.5361 | **0.5739** | 0.6301 | 1.2% faster |

Only `schedule(static)` — one contiguous block per thread — is meaningfully
slower, and that is the same effect measured independently in §3.2. The other
three are indistinguishable (their spread of 1.8% sits well inside the 12–18%
run-to-run variability).

**We kept `schedule(static, 1)`**: it matches the best measured performance
and has *zero* scheduling overhead, since the assignment is computed by
formula. `dynamic` and `guided` coordinate through a shared counter at
runtime and buy nothing here.

> A single run initially suggested `guided` was 9.7% faster. Repeating it five
> times made the difference disappear — a reminder of why we repeat.

---

## 4. Graphs

All in `graphs/`, 200 dpi PNG, regenerate with `python plot.py`.

| File | Spec graph |
|---|---|
| `graph0_task1_optimisation.png` | (extra, Task 1 section) |
| `graph1_runtime_vs_n_pthread.png` | 1 — run time serial vs pthread, increasing n |
| `graph2_speedup_vs_n_pthread.png` | 2 — speed-up pthread, increasing n |
| `graph3_runtime_vs_threads_pthread.png` | 3 — run time serial vs pthread, increasing threads |
| `graph4_speedup_vs_threads_pthread.png` | 4 — speed-up pthread, increasing threads |
| `graph5_runtime_vs_n_openmp.png` | 5 — run time serial vs OpenMP, increasing n |
| `graph6_speedup_vs_n_openmp.png` | 6 — speed-up OpenMP, increasing n |
| `graph7_runtime_vs_n_pthread_openmp.png` | 7 — pthread vs OpenMP, increasing n |
| `graph8_runtime_vs_threads_pthread_openmp.png` | 8 — pthread vs OpenMP, increasing threads |

Colours are fixed across every chart: **blue = serial, orange = POSIX
Threads, green = OpenMP**.

---

## 5. Files

| File | What it is |
|---|---|
| `task1.c` `task2.c` `task3.c` | **The submitted programs.** Unchanged since the cyclic fix. |
| `benchmark.sh` | Runs the ~350 measurements, writes `results.csv` |
| `summarize.sh` | Aggregates to `speedup.csv` (best-of-N + speed-up) |
| `plot.py` | Renders all nine graphs from `speedup.csv` / `optimisation.csv` |
| `optbench.c` `optbench.sh` | Task 1 optimisation experiment → `optimisation.csv` |
| `task3_sched.c` | Experiment copy of task3 using `schedule(runtime)` — **not for submission** |
| `results.csv` | Raw timings, current (cyclic) code |
| `results_contiguous.csv` | Raw timings, old contiguous partitioning — the "before" data |
| `speedup.csv` | Aggregated, ready to graph |
| `optimisation.csv` | Raw timings for the three primality-test versions |
| `commands.txt` | Hand-run verification commands |

Reproduce everything from scratch:

```bash
gcc -O2 -Wall -o task1 task1.c -lm
gcc -O2 -Wall -o task2 task2.c -pthread -lm
gcc -O2 -Wall -fopenmp -o task3 task3.c -lm
gcc -O2 -Wall -o optbench optbench.c -lm
bash benchmark.sh && bash summarize.sh   # ~15-20 min
bash optbench.sh                         # ~2 min
python plot.py
```

---

## 6. Answers to the questions the spec asks

**Is the speed-up equal to the number of threads? Why not — give at least two reasons.**

No. With 8 threads we measure ~4.7–5.0×, not 8×.

1. **Only 4 physical cores.** The other 4 logical CPUs are SMT siblings
   sharing one core's execution units — in particular the integer divider,
   which `is_prime` hammers. SMT adds roughly 20% on top of 4×, not another 4×.
2. **A sequential portion remains** (Amdahl's law). The `results[]` array is
   allocated, and compacted into the final sorted list, by a single thread —
   an O(n) pass that does not shrink with more threads. This is also why
   1 thread is slower than the serial program.
3. **Thread creation and placement overhead** — `pthread_create`/`join` cost
   is inside the timed region, and the OS does not place threads on distinct
   physical cores (measured: 13.3%, §3.5).

**Would you recommend OpenMP over POSIX Threads?**

For performance, no — once both use the same partitioning they are identical
within noise (§3.3). The choice is about effort and control:

- OpenMP replaced ~25 lines of manual thread management with one pragma, and
  its `schedule` clause let us test four work-distribution strategies by
  changing one word.
- POSIX Threads gave finer control and had markedly lower start-up cost at
  small n, where libgomp's thread-pool construction dominates.

We would use OpenMP for a loop like this one, and POSIX Threads where the
parallel structure is not a simple loop.

**How is the workload distributed, and is it a good approach?**

Cyclic (round-robin) over the odd numbers. `is_prime(k)` costs roughly √k, so
contiguous blocks leave the highest-numbered thread with much more work;
interleaving gives every thread a near-identical mix. Measured gain: **+18 to
+23% at large n** (§3.2). It is a good approach here because the cost varies
*predictably* with k — no runtime load balancing is needed, so a static
schedule with zero overhead is enough, which the `dynamic`/`guided`
comparison confirms (§3.6).

**What happens beyond the core count?**

Speed-up peaks at 10 threads (4.84×) and then plateaus — 4.76× at 16, 4.71×
at 32. No collapse: the work is compute-bound, so extra threads simply
time-share the same cores and the added context switching costs little.

---

## 7. Still to do

- Task 4 slides (6–8 min: Task 1 ≈ 1 min, Task 2 ≈ 3 min, Task 3 ≈ 2 min,
  conclusion ≈ 1 min)
- AI declaration PDF with prompt records — required by the spec
- Decide whether to also measure on the i7-12650H and, if so, present it as a
  clearly separate architecture comparison
