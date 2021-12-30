/** Examples of functions that can be used to create Worker instances and a random Worker factory function. **/

#ifndef WORKERS_MANAGER_EXAMPLE_WORKERS_H
#define WORKERS_MANAGER_EXAMPLE_WORKERS_H

#include <chrono>
#include <cmath>

#include "worker.h"

/** Dummy worker with a loop and sleep */
void dummy_worker(worker::yield_function_t yield, int loop_n, int sleep_ms) {
    for (int i = 0; i < loop_n; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

        if (!yield(i / static_cast<double>(loop_n))) {
            break;
        }
    }
}


/** Worker to compute n-th fibonacci number. Returns invalid result if stopped preemptively. Extremely inefficient */
std::uint64_t fibonacci_slow(const worker::yield_function_t& yield, int n) {
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return 1;
    }
    if (!yield(0)) { // no progress updates
        return -1;
    }
    return fibonacci_slow(yield, n - 1) + fibonacci_slow(yield, n - 2);
}

/** Simple selection sort */
template<class Iterator>
void selection_sort(const worker::yield_function_t& yield, Iterator first, Iterator last) {
    auto distance = std::distance(first, last);

    int n_sorted = 0;
    for (Iterator it = first; it != last; ++it) {
        std::iter_swap(it, std::min_element(it, last));
        ++n_sorted;

        if (!yield(n_sorted / distance)) {
            break;
        }
    }
}

#endif //WORKERS_MANAGER_EXAMPLE_WORKERS_H
