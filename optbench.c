
/* ----------------------------------------------------------------------
 * optbench.c - measures the effect of the primality-test optimisations.
 *
 * Three versions of the same test are selected at run time, so main(), the
 * scan over [2, n) and the timing code are identical between them and the
 * only thing that differs is is_prime itself:
 *
 *   1  naive    : trial division by every i in [2, k)
 *   2  sqrt     : trial division by every i in [2, sqrt(k)]
 *   3  sqrt+odd : evens rejected up front, then odd i in [3, sqrt(k)]
 *
 * Version 3 is the one shipped in task1.c, task2.c and task3.c.
 *
 * Only the COUNT of primes is produced - the list is never stored - so the
 * measurement isolates the cost of the primality test and is not affected
 * by allocation or memory traffic. All three versions must report the same
 * count for a given n; that is the correctness check.
 *
 * Build:
 *   gcc -O2 -Wall -o optbench optbench.c -lm
 * Usage:
 *   ./optbench <n> <version 1|2|3>
 *
 * WARNING: version 1 performs O(k) divisions per prime instead of
 * O(sqrt(k)), making it hundreds of times slower. Keep n <= 1000000 for
 * version 1 unless you are prepared to wait minutes.
 * ---------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

/* v1: the obvious approach - try every divisor below k. */
bool is_prime_naive(long long k) {
    if (k < 2) return false;
    for (long long i = 2; i < k; i++) {
        if (k % i == 0) return false;
    }
    return true;
}

/* v2: stop at sqrt(k). If k = m * n with m > sqrt(k), then n < sqrt(k),
 * so any composite is guaranteed to have a factor at or below sqrt(k). */
bool is_prime_sqrt(long long k) {
    if (k < 2) return false;
    long long limit = (long long) sqrt((double) k);
    for (long long i = 2; i <= limit; i++) {
        if (k % i == 0) return false;
    }
    return true;
}

/* v3: reject even k immediately, then test only odd divisors - halving
 * the number of divisions again. This is the version used in task1-3. */
bool is_prime_sqrt_odd(long long k) {
    if (k < 2) return false;
    if (k == 2) return true;
    if (k % 2 == 0) return false;

    long long limit = (long long) sqrt((double) k);
    for (long long i = 3; i <= limit; i += 2) {
        if (k % i == 0) return false;
    }
    return true;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <n> <version 1|2|3>\n", argv[0]);
        fprintf(stderr, "  1 = naive (divide by every i < k)\n");
        fprintf(stderr, "  2 = sqrt cutoff\n");
        fprintf(stderr, "  3 = sqrt cutoff + odd divisors only\n");
        return EXIT_FAILURE;
    }

    long long n = atoll(argv[1]);
    int version = atoi(argv[2]);

    if (n < 2) {
        fprintf(stderr, "Error: n must be >= 2.\n");
        return EXIT_FAILURE;
    }

    bool (*is_prime)(long long);
    const char *label;

    switch (version) {
        case 1: is_prime = is_prime_naive;    label = "naive";    break;
        case 2: is_prime = is_prime_sqrt;     label = "sqrt";     break;
        case 3: is_prime = is_prime_sqrt_odd; label = "sqrt+odd"; break;
        default:
            fprintf(stderr, "Error: version must be 1, 2 or 3.\n");
            return EXIT_FAILURE;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    long long count = 0;
    for (long long k = 2; k < n; k++) {
        if (is_prime(k)) count++;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("n = %lld | version = %d (%s) | primes found = %lld | time taken = %.6f seconds\n",
           n, version, label, count, elapsed);

    return EXIT_SUCCESS;
}
