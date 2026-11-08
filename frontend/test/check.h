/* Minimal test helpers: CHECK counts failures, test main returns the count. */
#pragma once
#include <stdio.h>
static int check_failures;
#define CHECK(e) do { if (!(e)) { check_failures++; fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #e); } } while (0)
#define CHECK_STR(a, b) do { const char *_a = (a), *_b = (b); if (strcmp(_a, _b)) { check_failures++; \
    fprintf(stderr, "%s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, _a, _b); } } while (0)
#define CHECK_INT(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { check_failures++; \
    fprintf(stderr, "%s:%d: %s = %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)
#define TEST_END() return check_failures ? 1 : 0
