// uses: frontend/ratecontrol.c
#include <string.h>
#include <math.h>
#include "check.h"
#include "ratecontrol.h"

#define NEAR(a, b) CHECK(fabs((a) - (b)) < 1e-9)

int main(void) {
    NEAR(rate_adjust(1000, 1000), 1.0);
    NEAR(rate_adjust(0, 1000), 1.005);          /* queue empty: produce more */
    NEAR(rate_adjust(2000, 1000), 0.995);       /* queue double: produce less */
    NEAR(rate_adjust(10000, 1000), 0.995);      /* clamped */
    NEAR(rate_adjust(500, 1000), 1.0025);
    TEST_END();
}
