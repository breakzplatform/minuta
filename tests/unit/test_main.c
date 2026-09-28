#include <stdio.h>
#include <string.h>

#include "check.h"
#include "tests.h"

int check_failures = 0;

void check_fail(const char *file, int line, const char *message)
{
    check_failures++;
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, message);
}

static void print_escaped(const char *text, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        unsigned char c = (unsigned char)text[i];

        if (c == '\n') {
            fputs("\\n", stderr);
        } else if (c == '\r') {
            fputs("\\r", stderr);
        } else if (c < 0x20 || c >= 0x7F) {
            fprintf(stderr, "\\x%02X", c);
        } else {
            fputc(c, stderr);
        }
    }
}

int check_bytes(const char *file, int line, const char *actual, size_t actual_size,
                const char *expected, size_t expected_size)
{
    if (actual != NULL && actual_size == expected_size
        && memcmp(actual, expected, expected_size) == 0) {
        return 1;
    }
    check_failures++;
    fprintf(stderr, "%s:%d: bytes differ\n  expected: \"", file, line);
    print_escaped(expected, expected_size);
    fputs("\"\n  actual:   \"", stderr);
    if (actual != NULL) {
        print_escaped(actual, actual_size);
    } else {
        fputs("(null)", stderr);
    }
    fputs("\"\n", stderr);
    return 0;
}

int main(int argc, char **argv)
{
    size_t i;
    int ran = 0;

    for (i = 0; all_tests[i].name != NULL; i++) {
        int before = check_failures;

        if (argc > 1 && strstr(all_tests[i].name, argv[1]) == NULL) {
            continue;
        }
        all_tests[i].run();
        ran++;
        if (check_failures != before) {
            fprintf(stderr, "FAIL %s\n", all_tests[i].name);
        }
    }
    printf("%d tests, %d failed checks\n", ran, check_failures);
    return check_failures == 0 ? 0 : 1;
}
