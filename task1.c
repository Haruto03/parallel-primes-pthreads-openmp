#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#define OUTPUT_FILE "primes_output.txt"
#define STDOUT_THRESHOLD 100  /* n < 100 -> stdout, n >= 100 -> file */

/* ----------------------------------------------------------------------
 * is_prime: returns true if k is a prime number, false otherwise.
 * Uses trial division up to sqrt(k), skipping even divisors.
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
 * estimate_capacity: initial size for the prime buffer, based on the
 * prime number theorem (pi(n) ~ n / ln(n)). The 1.2 factor keeps this
 * above the true count of primes below n across the range this lab
 * exercises, so the buffer normally never has to grow during the timed
 * section. Small n falls back to a fixed minimum since the estimate is
 * unreliable (and irrelevant memory-wise) down there.
 * ---------------------------------------------------------------------- */
long long estimate_capacity(long long n) {
    if (n < 1000) return 1000;
    long long capacity = (long long) (1.2 * (double) n / log((double) n));
    return (capacity > 1000) ? capacity : 1000;
}

/* ----------------------------------------------------------------------
 * find_primes: scans [2, n) and stores every prime found into *primes,
 * growing the buffer with realloc if the estimate undershoots. The array
 * is filled in increasing order of k, so the result is already sorted in
 * ascending order - no extra sorting step is required.
 * Returns the number of primes found, or -1 if a reallocation failed.
 * ---------------------------------------------------------------------- */
long long find_primes(long long n, long long **primes, long long capacity) {
    long long count = 0;
    for (long long k = 2; k < n; k++) {
        if (is_prime(k)) {
            if (count >= capacity) {
                capacity *= 2;
                long long *temp = realloc(*primes, capacity * sizeof(long long));
                if (temp == NULL) {
                    fprintf(stderr, "Error: memory reallocation failed.\n");
                    return -1;
                }
                *primes = temp;
            }
            (*primes)[count++] = k;
        }
    }
    return count;
}

/* ----------------------------------------------------------------------
 * write_primes: outputs the sorted prime list either to stdout (small n)
 * or to a text file (large n), one prime per line, plus a summary.
 * ---------------------------------------------------------------------- */
void write_primes(long long n, long long *primes, long long count, double elapsed) {
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

    printf("n = %lld | primes found = %lld | time taken = %.6f seconds\n",
           n, count, elapsed);
}

int main(int argc, char *argv[]) {
    long long n;

    /* Accept n either as a command-line argument (for easy benchmark
     * scripting across many n values, as required for Tasks 2 & 3) or
     * interactively from the terminal. */
    if (argc >= 2) {
        n = atoll(argv[1]);
    } else {
        printf("Enter n (find primes strictly less than n): ");
        if (scanf("%lld", &n) != 1) {
            fprintf(stderr, "Error: invalid input.\n");
            return EXIT_FAILURE;
        }
    }

    if (n < 2) {
        printf("There are no prime numbers less than %lld.\n", n);
        return EXIT_SUCCESS;
    }

    long long capacity = estimate_capacity(n);
    long long *primes = malloc(capacity * sizeof(long long));
    if (primes == NULL) {
        fprintf(stderr, "Error: memory allocation failed for n = %lld.\n", n);
        return EXIT_FAILURE;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    long long count = find_primes(n, &primes, capacity);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                      (end.tv_nsec - start.tv_nsec) / 1e9;

    /*When reallocating memory is failed in find_primes, count will be -1*/                  
    if (count < 0) {
        free(primes);
        return EXIT_FAILURE;
    }

    write_primes(n, primes, count, elapsed);

    free(primes);
    return EXIT_SUCCESS;
}
