#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <omp.h>

#define OUTPUT_FILE "primes_output_openmp.txt"
#define STDOUT_THRESHOLD 100  /* n < 100 -> stdout, n >= 100 -> file */

/* ----------------------------------------------------------------------
 * is_prime: identical primality test to task1.c/task2.c (trial division
 * up to sqrt(k), skipping even divisors), so all three versions do the
 * same amount of "real" work per number.
 * ---------------------------------------------------------------------- */
bool is_prime(long long k) {
    if (k < 2) return false;
    if (k == 2) return true;
    if (k % 2 == 0) return false;

    long long limit = (long long) sqrt((double) k);
    for (long long i = 3; i <= limit; i += 2) {
        if (k % i == 0) return false;
    }
    return true;
}

/* ----------------------------------------------------------------------
 * write_primes: same output format/behaviour as task1.c/task2.c, so
 * results can be diffed directly against both for correctness checks.
 * ---------------------------------------------------------------------- */
void write_primes(long long n, int num_threads, long long *primes, long long count, double elapsed) {
    if (n < STDOUT_THRESHOLD) {
        printf("\nPrime numbers less than %lld (%lld found):\n", n, count);
        for (long long i = 0; i < count; i++) {
            printf("%lld", primes[i]);
            if (i != count - 1) printf(", ");
        }
        printf("\n");
    } else {
        FILE *fp = fopen(OUTPUT_FILE, "w");
        if (fp == NULL) {
            fprintf(stderr, "Error: could not open %s for writing.\n", OUTPUT_FILE);
            return;
        }
        fprintf(fp, "Prime numbers less than %lld (%lld found):\n", n, count);
        for (long long i = 0; i < count; i++) {
            fprintf(fp, "%lld\n", primes[i]);
        }
        fclose(fp);
        printf("\n%lld primes written to \"%s\"\n", count, OUTPUT_FILE);
    }

    printf("n = %lld | threads = %d | primes found = %lld | time taken = %.6f seconds\n",
           n, num_threads, count, elapsed);
}

int main(int argc, char *argv[]) {
    long long n;
    int num_threads;

    if (argc >= 3) {
        n = atoll(argv[1]);
        num_threads = atoi(argv[2]);
    } else {
        printf("Enter n (find primes strictly less than n): ");
        if (scanf("%lld", &n) != 1) {
            fprintf(stderr, "Error: invalid input for n.\n");
            return EXIT_FAILURE;
        }
        printf("Enter number of threads: ");
        if (scanf("%d", &num_threads) != 1) {
            fprintf(stderr, "Error: invalid input for num_threads.\n");
            return EXIT_FAILURE;
        }
    }

    if (n < 2) {
        printf("There are no prime numbers less than %lld.\n", n);
        return EXIT_SUCCESS;
    }
    if (num_threads < 1) {
        fprintf(stderr, "Error: number of threads must be >= 1.\n");
        return EXIT_FAILURE;
    }

    omp_set_num_threads(num_threads);

    /* Shared output array: results[k] = k if k is prime, else 0 (calloc). */
    long long *results = calloc(n, sizeof(long long));
    if (results == NULL) {
        fprintf(stderr, "Error: memory allocation failed for n = %lld.\n", n);
        return EXIT_FAILURE;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* k = 2 is the only even prime; handle it directly so the parallel
     * loop below can restrict itself to odd numbers only. */
    if (n > 2) {
        results[2] = 2;
    }

    /* Number of odd numbers in [3, n): 3, 5, 7, ..., largest odd < n. */
    long long num_odds = 0;
    if (n > 3) {
        long long last_odd = (n % 2 == 0) ? (n - 1) : (n - 2);
        if (last_odd >= 3) {
            num_odds = (last_odd - 3) / 2 + 1;
        }
    }

    /* Parallel search: schedule(static, 1) gives round-robin (cyclic)
     * assignment of odd-number INDICES to threads, matching task2.c's
     * manual cyclic partitioning. Each thread only ever writes results[k]
     * for the k values it owns, so no synchronization is needed here. */
    long long idx;
    #pragma omp parallel for schedule(static, 1)
    for (idx = 0; idx < num_odds; idx++) {
        long long k = 3 + 2 * idx;
        if (is_prime(k)) {
            results[k] = k;
        }
    }

    /* Sequential compaction: results[] is already in ascending order of k,
     * so this single pass produces the final sorted prime list. */
    long long *primes = malloc(n * sizeof(long long));
    long long count = 0;
    if (primes != NULL) {
        for (long long k = 2; k < n; k++) {
            if (results[k] != 0) {
                primes[count++] = results[k];
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                      (end.tv_nsec - start.tv_nsec) / 1e9;

    write_primes(n, num_threads, primes, count, elapsed);

    free(results);
    free(primes);
    return EXIT_SUCCESS;
}
