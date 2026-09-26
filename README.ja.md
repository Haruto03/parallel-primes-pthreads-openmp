# 並列素数探索: 逐次・POSIX Threads・OpenMP

[English](README.md) | 日本語

*n* 未満のすべての素数を求める 3 つの C プログラムです。同一のワークロードに対して、
共有メモリ並列化のアプローチを比較する目的で書きました。

| プログラム | 手法 | 仕事の分配 |
|---|---|---|
| `task1.c` | 逐次(ベースライン) | — |
| `task2.c` | POSIX Threads | 循環分配: スレッド *t* が `3 + 2t`、`3 + 2t + 2T`、… を判定 |
| `task3.c` | OpenMP | `#pragma omp parallel for schedule(static, 1)` による循環分配 |

3 つとも同一の素数判定(√k までの試し割り、奇数の除数のみ)を共有しているため、
計測される差は並列化の違いだけに由来します。並列版はどちらもロックを必要としません。
結果は判定対象の値をインデックスとするスロットに書き込むため、複数スレッドが同じメモリに
触れることがないからです。

Haruto Iriyama が 2 人チームの大学プロジェクトとして制作しました。続編として、
同じ問題を分散メモリ環境の Open MPI と MPI + OpenMP のハイブリッドで解いたものが
[parallel-primes-mpi](https://github.com/Haruto03/parallel-primes-mpi) にあります。

## 主な知見

すべての数値を含む詳細は [RESULTS.md](RESULTS.md) にまとめています。

- **アルゴリズムの改善は、並列化より 3 桁効いた。** *k − 1* までの素朴な試し割りは
  n = 10⁶ で 113.8 秒かかりますが、√k で打ち切って偶数の除数を飛ばすだけで*逐次*版が
  0.104 秒になります。これは素朴な実装を完璧に 8 並列化したとしても届かない速さです。
- **ボトルネックはスレッド API ではなく分配方法だった。** 連続ブロックで分割すると、
  大きな数を担当するスレッドが最後まで残ってしまいます。循環分配に変えたところ、
  大きな n で 18〜23 % の高速化が得られました。
- **同じ分配方法を使えば POSIX Threads と OpenMP の性能は同等**(差は約 3 % 以内)です。
  当初見えていた OpenMP の 20 % の優位は、すべて分配方法の違いによるものでした。
- 論理 8 コア / 物理 4 コアでは高速化率が 4.5〜5 倍で頭打ちになります。SMT の兄弟スレッドが
  実行ユニットを共有するためです。

![スレッド数に対する高速化率](graphs/graph8_runtime_vs_threads_pthread_openmp.png)

## ビルドと実行

```bash
gcc -O2 -Wall -o task1 task1.c -lm
gcc -O2 -Wall -o task2 task2.c -pthread -lm
gcc -O2 -Wall -fopenmp -o task3 task3.c -lm

./task1 10000000          # primes_output.txt を出力
./task2 10000000 8        # 8 スレッド、primes_output_parallel.txt を出力
./task3 10000000 8        # primes_output_openmp.txt を出力
./verify.sh               # 3 つの出力が一致するか検証
```

100 未満の数値は標準出力に表示し、それより大きい実行ではファイルに書き出します。

## 計測の再現

```bash
gcc -O2 -Wall -o optbench optbench.c -lm
bash benchmark.sh && bash summarize.sh   # 約 350 回の実行、15〜20 分 -> results.csv, speedup.csv
bash optbench.sh                         # 素数判定の比較 -> optimisation.csv
python plot.py                           # graphs/*.png を描画(matplotlib が必要)
```

| ファイル | 用途 |
|---|---|
| `benchmark.sh`, `summarize.sh` | n(5×10⁷ までの 30 通り)とスレッド数(1〜32)を掃引し、3 回中のベストを採用 |
| `optbench.c`, `optbench.sh` | 同一のハーネス上で 3 種類の素数判定を比較 |
| `task3_sched.c` | 実験用: `schedule(runtime)` で OpenMP のスケジュールを比較 |
| `results.csv`, `results_contiguous.csv` | 循環分配への修正後 / 修正前の生の計測値 |
| `speedup.csv`, `optimisation.csv` | グラフの元になる集計データ |
| `graphs/` | 9 枚のグラフ |
| `commands.txt` | 手動で実行した検証コマンド |

計測は AMD Ryzen 5 7535HS(4 コア / 8 スレッド)上の Linux(Ubuntu)Docker コンテナ内で
行いました。
