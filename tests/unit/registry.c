#include <stddef.h>

#include "tests.h"

#define TEST(name) void name(void);
#include "test_list.h"
#undef TEST

#define TEST(name) { #name, name },
const struct test_case all_tests[] = {
#include "test_list.h"
    { NULL, NULL }
};
