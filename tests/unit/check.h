#ifndef MINUTA_CHECK_H
#define MINUTA_CHECK_H

#include <stddef.h>

/* Minimal test harness: each test file registers its cases in main.c. */

extern int check_failures;

void check_fail(const char *file, int line, const char *message);
int check_bytes(const char *file, int line, const char *actual, size_t actual_size,
                const char *expected, size_t expected_size);

#define CHECK(condition) \
    do { if (!(condition)) check_fail(__FILE__, __LINE__, #condition); } while (0)

#define CHECK_BYTES(actual, actual_size, expected, expected_size) \
    check_bytes(__FILE__, __LINE__, (actual), (actual_size), (expected), (expected_size))

#define CHECK_STR(actual, actual_size, expected) \
    check_bytes(__FILE__, __LINE__, (actual), (actual_size), (expected), strlen(expected))

typedef void (*test_function)(void);

struct test_case {
    const char *name;
    test_function run;
};

#endif
