#include "FastLinkedList.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static uint64_t now_us(void) {
    static LARGE_INTEGER freq;
    static int init = 0;
    LARGE_INTEGER t;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }
    QueryPerformanceCounter(&t);
    return (uint64_t)((t.QuadPart * 1000000ULL) / (uint64_t)freq.QuadPart);
}
#else
#include <time.h>
static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}
#endif

static FastLinkedList* build_list(int n, unsigned seed) {
    FastLinkedList *list = createList();
    srand(seed);
    for (int i = 0; i < n; i++) {
        insert(list, list->size, rand());
    }
    return list;
}

static void write_row(FILE *fp, int n, const char *op, int trial, int ops, uint64_t us) {
    double total_us = (double)us;
    double avg_us = total_us / (double)ops;
    fprintf(fp, "%d,%s,%d,%d,%.3f,%.6f\n", n, op, trial, ops, total_us, avg_us);
}

static void bench_insert(FILE *fp, int n, int ops, const char *opname, int mode, int trial) {
    FastLinkedList *list = build_list(n, (unsigned)(12345u + 1000u * (unsigned)trial + (unsigned)n));
    for (int i = 0; i < 32; i++) insert(list, list->size, rand());

    uint64_t t0 = now_us();
    for (int i = 0; i < ops; i++) {
        int pos;
        if (mode == 0) pos = 0;
        else if (mode == 1) pos = list->size;
        else pos = (list->size == 0) ? 0 : (rand() % (list->size + 1));

        insert(list, pos, rand());
    }
    uint64_t t1 = now_us();

    write_row(fp, n, opname, trial, ops, t1 - t0);
    destroyList(list);
}

static void bench_remove(FILE *fp, int n, int ops, const char *opname, int mode, int trial) {
    FastLinkedList *list = build_list(n + ops + 64, (unsigned)(54321u + 2000u * (unsigned)trial + (unsigned)n));

    volatile int sink = 0;
    for (int i = 0; i < 128; i++) sink ^= get(list, rand() % list->size);

    uint64_t t0 = now_us();
    for (int i = 0; i < ops; i++) {
        int pos;
        if (mode == 0) pos = 0;
        else if (mode == 1) pos = list->size - 1;
        else pos = (list->size <= 1) ? 0 : (rand() % list->size);

        removeAt(list, pos);
    }
    uint64_t t1 = now_us();
    (void)sink;

    write_row(fp, n, opname, trial, ops, t1 - t0);
    destroyList(list);
}

static void bench_get(FILE *fp, int n, int ops, const char *opname, int trial) {
    FastLinkedList *list = build_list(n, (unsigned)(99991u + 3000u * (unsigned)trial + (unsigned)n));

    volatile int sink = 0;
    for (int i = 0; i < 256; i++) sink ^= get(list, rand() % list->size);

    uint64_t t0 = now_us();
    for (int i = 0; i < ops; i++) {
        sink ^= get(list, rand() % list->size);
    }
    uint64_t t1 = now_us();
    (void)sink;

    write_row(fp, n, opname, trial, ops, t1 - t0);
    destroyList(list);
}

int main(void) {
    const int sizes[] = {1000, 2000, 5000, 10000, 20000};
    const int num_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));
    const int trials = 5;

    const int OPS_INSERT = 2000;
    const int OPS_REMOVE = 2000;
    const int OPS_GET    = 20000;

    FILE *fp = fopen("results.csv", "w");
    if (!fp) {
        perror("results.csv");
        return 1;
    }

    fprintf(fp, "n,operation,trial,ops,total_us,avg_us\n");

    for (int si = 0; si < num_sizes; si++) {
        int n = sizes[si];

        for (int tr = 1; tr <= trials; tr++) {
            bench_insert(fp, n, OPS_INSERT, "insert_front", 0, tr);
            bench_insert(fp, n, OPS_INSERT, "insert_end",   1, tr);
            bench_insert(fp, n, OPS_INSERT, "insert_rand",  2, tr);

            bench_remove(fp, n, OPS_REMOVE, "remove_front", 0, tr);
            bench_remove(fp, n, OPS_REMOVE, "remove_end",   1, tr);
            bench_remove(fp, n, OPS_REMOVE, "remove_rand",  2, tr);

            bench_get(fp, n, OPS_GET, "get_rand", tr);
        }

        fflush(fp);
        printf("Done n=%d\n", n);
    }

    fclose(fp);
    printf("Wrote results.csv\n");
    return 0;
}
