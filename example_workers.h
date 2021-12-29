#ifndef WORKERS_MANAGER_EXAMPLE_WORKERS_H
#define WORKERS_MANAGER_EXAMPLE_WORKERS_H

#include "worker.h"

/** Worker to compute n-th fibonacci number. Extremely inefficient. */
std::uint64_t fibonacci_slow(const worker::yield_function_t& yield, int n) {
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return 1;
    }
    yield(0); // no progress updates
    return fibonacci_slow(yield, n - 1) + fibonacci_slow(yield, n - 2);
}

#endif //WORKERS_MANAGER_EXAMPLE_WORKERS_H
