#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#define OUTPUT_FILE "primes_output_parallel.txt"
#define STDOUT_THRESHOLD 100  /* n < 100 -> stdout, n >= 100 -> file */

/* ----------------------------------------------------------------------
 * is_prime: identical primality test to task1.c (trial division up to
 * sqrt(k), skipping even divisors) so the two versions are doing exactly
 * the same amount of "real" work per number.
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

/* Arguments passed to each worker thread. */
typedef struct {
    int thread_id;      /* 0 .. num_threads-1                         */
    int num_threads;    /* stride for cyclic partitioning              */
    long long n;         /* search primes strictly less than n          */
    long long *results;  /* shared output array, size n (index = value) */
} thread_arg_t;

/* ----------------------------------------------------------------------
 * thread_func: cyclic (round-robin) partitioning over the odd numbers.
 * Thread t starts at k = 3 + 2t and strides by 2 * num_threads.
 *
 * Why cyclic and not a contiguous block per thread: the cost of is_prime(k)
 * grows with sqrt(k), so splitting [2, n) into equal-width blocks would give
 * the thread owning the highest block about 40% more work than the average,
 * capping the speed-up well below the thread count. Interleaving the numbers
 * gives every thread a near-identical mix of cheap and expensive values.
 *
 * k = 2 is the only even prime and is handled in main, so even numbers are
 * skipped entirely here. Threads never write to the same index, so no
 * locking is required.
 * ---------------------------------------------------------------------- */
void *thread_func(void *arg) {
    thread_arg_t *targ = (thread_arg_t *) arg;

    for (long long k = 3 + 2 * targ->thread_id; k < targ->n;
         k += 2 * targ->num_threads) {
        if (is_prime(k)) {
            targ->results[k] = k;
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------------
 * write_primes: same output format/behaviour as task1.c, so results can
 * be diffed directly against the serial version for correctness checks.
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

    /* Shared output array: results[k] = k if k is prime, else 0 (calloc). */
    long long *results = calloc(n, sizeof(long long));
    if (results == NULL) {
        fprintf(stderr, "Error: memory allocation failed for n = %lld.\n", n);
        return EXIT_FAILURE;
    }

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    thread_arg_t *targs = malloc(num_threads * sizeof(thread_arg_t));
    if (threads == NULL || targs == NULL) {
        fprintf(stderr, "Error: memory allocation failed for %d threads.\n", num_threads);
        free(results);
        return EXIT_FAILURE;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* k = 2 is the only even prime; the worker threads scan odd numbers only. */
    if (n > 2) {
        results[2] = 2;
    }

    /* Spawn threads: each gets its thread id + shared context. */
    for (int t = 0; t < num_threads; t++) {
        targs[t].thread_id = t;
        targs[t].num_threads = num_threads;
        targs[t].n = n;
        targs[t].results = results;

        int rc = pthread_create(&threads[t], NULL, thread_func, &targs[t]);
        if (rc != 0) {
            fprintf(stderr, "Error: pthread_create failed for thread %d (code %d).\n", t, rc);
            return EXIT_FAILURE;
        }
    }

    /* Wait for all threads to finish before compacting results. */
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
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
    free(threads);
    free(targs);
    free(primes);
    return EXIT_SUCCESS;
}
