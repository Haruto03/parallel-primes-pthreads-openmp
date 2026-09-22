#!/usr/bin/env python3
"""
plot.py - Produce the graphs used in RESULTS.md.

Reads speedup.csv (written by summarize.sh) and writes one PNG per graph
into graphs/.

    python plot.py [speedup.csv]

Graphs, numbered as in the assessment specification:

  Task 2 section                      Task 3 section
  1  runtime serial vs pthread, x=n   5  runtime serial vs OpenMP,  x=n
  2  speedup pthread,          x=n    6  speedup OpenMP,            x=n
  3  runtime serial vs pthread, x=T   7  runtime pthread vs OpenMP, x=n
  4  speedup pthread,           x=T   8  runtime pthread vs OpenMP, x=T
"""

import csv
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

# --- palette -----------------------------------------------------------
# Categorical slots 1-3: identity (which program), never magnitude.
# Colour follows the entity and is identical in every chart, so a reader
# who learns "orange is POSIX Threads" is never contradicted.
C_SERIAL = "#2a78d6"   # slot 1, blue
C_PTHREAD = "#eb6834"  # slot 2, orange
C_OPENMP = "#1baf7a"   # slot 3, aqua

# The three primality-test versions are an ORDERED progression (each one is
# the previous plus an optimisation), so they take an ordinal ramp - one
# hue, light to dark - rather than three unrelated categorical hues. The
# darkest step is v3, the version actually shipped in task1-3.
C_V1 = "#86b6ef"       # blue step 250 - lightest step that still clears 2:1
C_V2 = "#2a78d6"       # blue step 450
C_V3 = "#0d366b"       # blue step 700

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_2 = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"

PHYSICAL_CORES = 4     # Ryzen 5 7535HS: 4 physical cores / 8 logical

LW = 2.0
MS = 5


def style(ax, title, xlabel, ylabel):
    """Recessive chrome: hairline solid grid, no top/right spine."""
    ax.set_title(title, color=INK, fontsize=12, fontweight="bold",
                 loc="left", pad=12)
    ax.set_xlabel(xlabel, color=INK_2, fontsize=10)
    ax.set_ylabel(ylabel, color=INK_2, fontsize=10)
    ax.grid(True, which="major", color=GRID, linewidth=0.8, linestyle="-")
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)
        ax.spines[side].set_linewidth(0.8)
    ax.tick_params(colors=MUTED, labelsize=9, length=0)


def new_fig():
    fig, ax = plt.subplots(figsize=(8, 5))
    fig.patch.set_facecolor(SURFACE)
    ax.set_facecolor(SURFACE)
    return fig, ax


def legend(ax):
    leg = ax.legend(frameon=False, fontsize=9, loc="best")
    for text in leg.get_texts():
        text.set_color(INK_2)


def ref_line(ax, y, label, xpos):
    """A reference level (break-even, physical core count), not a series."""
    ax.axhline(y, color=MUTED, linewidth=1.0, linestyle=(0, (4, 3)), zorder=1)
    ax.annotate(label, xy=(xpos, y), xytext=(0, 4), textcoords="offset points",
                color=MUTED, fontsize=8, ha="right", va="bottom")


def n_fmt(v, _):
    if v >= 1e6:
        return f"{v/1e6:g}M"
    if v >= 1e3:
        return f"{v/1e3:g}k"
    return f"{v:g}"


def save(fig, name):
    os.makedirs("graphs", exist_ok=True)
    path = os.path.join("graphs", name)
    fig.tight_layout()
    fig.savefig(path, dpi=200, facecolor=SURFACE)
    plt.close(fig)
    print("wrote", path)


def load(path):
    vary_n, vary_t = [], []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            rec = {
                "n": int(row["n"]),
                "threads": int(row["threads"]),
                "serial": float(row["serial_s"]),
                "pthread": float(row["pthread_s"]),
                "openmp": float(row["openmp_s"]),
                "su_pthread": float(row["speedup_pthread"]),
                "su_openmp": float(row["speedup_openmp"]),
            }
            (vary_n if row["experiment"] == "vary_n" else vary_t).append(rec)
    vary_n.sort(key=lambda r: r["n"])
    vary_t.sort(key=lambda r: r["threads"])
    return vary_n, vary_t


def runtime_vs_n(rows, keys, labels, colours, title, fname):
    fig, ax = new_fig()
    xs = [r["n"] for r in rows]
    for key, label, colour in zip(keys, labels, colours):
        ax.plot(xs, [r[key] for r in rows], color=colour, linewidth=LW,
                marker="o", markersize=MS, label=label)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.xaxis.set_major_formatter(FuncFormatter(n_fmt))
    style(ax, title, "n (search range, log scale)",
          "Run time in seconds (log scale)")
    legend(ax)
    save(fig, fname)


def speedup_vs_n(rows, key, label, colour, title, fname):
    fig, ax = new_fig()
    xs = [r["n"] for r in rows]
    ys = [r[key] for r in rows]
    ax.plot(xs, ys, color=colour, linewidth=LW, marker="o", markersize=MS,
            label=label, zorder=3)
    ax.set_xscale("log")
    ax.set_ylim(0, max(ys) * 1.25)
    ax.xaxis.set_major_formatter(FuncFormatter(n_fmt))
    ref_line(ax, 1.0, "break-even (speed-up = 1)", max(xs))
    ref_line(ax, PHYSICAL_CORES, f"{PHYSICAL_CORES} physical cores", max(xs))
    peak = max(rows, key=lambda r: r[key])
    ax.annotate(f"peak {peak[key]:.2f}x", xy=(peak["n"], peak[key]),
                xytext=(0, 10), textcoords="offset points",
                color=colour, fontsize=9, ha="center", fontweight="bold")
    style(ax, title, "n (search range, log scale)",
          "Speed-up (serial time / parallel time)")
    legend(ax)
    save(fig, fname)


def vs_threads(rows, series, title, ylabel, fname, ylim_pad=1.25,
               speedup=False):
    fig, ax = new_fig()
    xs = [r["threads"] for r in rows]
    top = 0
    for key, label, colour in series:
        ys = [r[key] for r in rows]
        top = max(top, max(ys))
        ax.plot(xs, ys, color=colour, linewidth=LW, marker="o",
                markersize=MS, label=label, zorder=3)
    ax.set_xscale("log", base=2)
    ax.set_xticks(xs)
    ax.set_xticklabels([str(x) for x in xs])
    ax.set_ylim(0, top * ylim_pad)
    if speedup:
        ref_line(ax, 1.0, "break-even", max(xs))
        ref_line(ax, PHYSICAL_CORES, f"{PHYSICAL_CORES} physical cores",
                 max(xs))
    ax.axvline(8, color=MUTED, linewidth=1.0, linestyle=(0, (2, 3)), zorder=1)
    ax.annotate("8 logical CPUs", xy=(8, top * ylim_pad), xytext=(4, -12),
                textcoords="offset points", color=MUTED, fontsize=8,
                ha="left", va="top", rotation=90)
    style(ax, title, "Number of threads (log2 scale)", ylabel)
    legend(ax)
    save(fig, fname)


def optimisation_chart(path="optimisation.csv"):
    """Task 1: what the primality-test optimisations actually bought.

    Best-of-N per (n, version) from optbench.sh. Log-log, so the difference
    in slope is the difference in complexity: the naive test costs O(k)
    divisions per prime, the other two O(sqrt(k)), and that gap widens with n.
    """
    if not os.path.exists(path):
        print(f"(skipping optimisation chart - {path} not found)")
        return

    best = {}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            key = (int(row["n"]), int(row["version"]))
            t = float(row["time_seconds"])
            if key not in best or t < best[key]:
                best[key] = t

    ns = sorted({n for n, _ in best})
    fig, ax = new_fig()
    for version, colour, label in ((1, C_V1, "v1  trial division to k-1"),
                                   (2, C_V2, "v2  stop at sqrt(k)"),
                                   (3, C_V3, "v3  sqrt(k), odd divisors only")):
        ys = [best[(n, version)] for n in ns]
        ax.plot(ns, ys, color=colour, linewidth=LW, marker="o",
                markersize=MS, label=label, zorder=3)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.xaxis.set_major_formatter(FuncFormatter(n_fmt))

    n_max = ns[-1]
    factor = best[(n_max, 1)] / best[(n_max, 3)]
    ax.annotate(f"v3 is {factor:,.0f}x faster than v1 here",
                xy=(n_max, best[(n_max, 3)]),
                xytext=(-6, -34), textcoords="offset points",
                color=C_V3, fontsize=10, fontweight="bold", ha="right")

    style(ax, "Task 1: effect of the primality-test optimisations",
          "n (search range, log scale)", "Run time in seconds (log scale)")
    legend(ax)
    save(fig, "graph0_task1_optimisation.png")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "speedup.csv"
    optimisation_chart()
    vary_n, vary_t = load(path)
    if not vary_n or not vary_t:
        sys.exit(f"{path}: missing vary_n or vary_threads rows")

    threads = vary_n[0]["threads"]
    n_fixed = vary_t[0]["n"]

    # --- Task 2 section -------------------------------------------------
    runtime_vs_n(
        vary_n, ["serial", "pthread"], ["Serial (Task 1)", f"POSIX Threads ({threads} threads)"],
        [C_SERIAL, C_PTHREAD],
        "1. Run time: serial vs POSIX Threads, increasing n",
        "graph1_runtime_vs_n_pthread.png")

    speedup_vs_n(
        vary_n, "su_pthread", f"POSIX Threads ({threads} threads)", C_PTHREAD,
        "2. Speed-up of POSIX Threads, increasing n",
        "graph2_speedup_vs_n_pthread.png")

    vs_threads(
        vary_t,
        [("serial", "Serial (Task 1)", C_SERIAL),
         ("pthread", "POSIX Threads", C_PTHREAD)],
        f"3. Run time: serial vs POSIX Threads, increasing threads (n = {n_fixed:,})",
        "Run time in seconds",
        "graph3_runtime_vs_threads_pthread.png")

    vs_threads(
        vary_t, [("su_pthread", "POSIX Threads", C_PTHREAD)],
        f"4. Speed-up of POSIX Threads, increasing threads (n = {n_fixed:,})",
        "Speed-up (serial time / parallel time)",
        "graph4_speedup_vs_threads_pthread.png", speedup=True)

    # --- Task 3 section -------------------------------------------------
    runtime_vs_n(
        vary_n, ["serial", "openmp"], ["Serial (Task 1)", f"OpenMP ({threads} threads)"],
        [C_SERIAL, C_OPENMP],
        "5. Run time: serial vs OpenMP, increasing n",
        "graph5_runtime_vs_n_openmp.png")

    speedup_vs_n(
        vary_n, "su_openmp", f"OpenMP ({threads} threads)", C_OPENMP,
        "6. Speed-up of OpenMP, increasing n",
        "graph6_speedup_vs_n_openmp.png")

    runtime_vs_n(
        vary_n, ["pthread", "openmp"],
        [f"POSIX Threads ({threads} threads)", f"OpenMP ({threads} threads)"],
        [C_PTHREAD, C_OPENMP],
        "7. Run time: POSIX Threads vs OpenMP, increasing n",
        "graph7_runtime_vs_n_pthread_openmp.png")

    vs_threads(
        vary_t,
        [("pthread", "POSIX Threads", C_PTHREAD),
         ("openmp", "OpenMP", C_OPENMP)],
        f"8. Run time: POSIX Threads vs OpenMP, increasing threads (n = {n_fixed:,})",
        "Run time in seconds",
        "graph8_runtime_vs_threads_pthread_openmp.png")


if __name__ == "__main__":
    main()
