#include "ratecontrol.h"

double rate_adjust(size_t queued, size_t target) {
    double r = 1.0 + 0.005 * ((double)target - (double)queued) / (double)target;
    return r < 0.995 ? 0.995 : r > 1.005 ? 1.005 : r;
}
