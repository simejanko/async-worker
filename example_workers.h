/** Examples of functions that can be used to create Worker instances and a random Worker factory function. **/

#ifndef WORKERS_MANAGER_EXAMPLE_WORKERS_H
#define WORKERS_MANAGER_EXAMPLE_WORKERS_H

#include <chrono>
#include <cmath>
#include <random>

#include "worker.h"

namespace worker {
    const std::vector<std::string> WORKER_EXAMPLES = {"dummy_worker", "fibonacci_slow", "selection_sort"};

    /** Dummy worker with a loop and sleep */
    void dummy_worker(yield_function_t yield, int loop_n, int sleep_ms) {
        for (int i = 0; i < loop_n; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

            if (!yield(i / static_cast<double>(loop_n))) {
                break;
            }
        }
    }

    /** Computes n-th fibonacci number. Returns invalid result if stopped preemptively. Extremely inefficient */
    std::uint64_t fibonacci_slow(const yield_function_t& yield, int n) {
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
    void selection_sort(yield_function_t yield, Iterator first, Iterator last) {
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

    /**
     * Factory function that returns random BaseWorker instances with random arguments, based on implementations in this file
     * @throws std::logic_error if worker that's not yet implemented in the factory is selected
     */
    std::shared_ptr<BaseWorker> random_worker() {
        // sample a random worker function from WORKER_EXAMPLES
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::size_t> distr(0, WORKER_EXAMPLES.size()-1);
        std::string worker_name = WORKER_EXAMPLES[distr(gen)];

        if (worker_name == "dummy_worker") {
            std::uniform_int_distribution<int> loop_n_distr(100, 10000), sleep_ms_distr(10, 100);
            // unfortunately pointers and template deductions don't play nice
            return std::make_shared<AsyncWorker<decltype(&dummy_worker), int, int>>(
                    &dummy_worker, loop_n_distr(gen), sleep_ms_distr(gen));
        }
        if (worker_name == "fibonacci_slow") {
            std::uniform_int_distribution<int> n_distr(40, 50);
            return std::make_shared<AsyncWorker<decltype(&fibonacci_slow), int>>(&fibonacci_slow, n_distr(gen));
        }
        if (worker_name == "selection_sort") {
            std::uniform_int_distribution<std::size_t> vec_size(1000, 1e6);
            std::uniform_int_distribution<int> vec_distr(-1e5, 1e5);
            std::vector<int> rand_vec(vec_size(gen));
            std::generate(rand_vec.begin(), rand_vec.end(), [&]() { return vec_distr(gen); });

            // wrap selection sort with lambda that returns sorted copy
            auto lambda =
                    [vec_copy = rand_vec](yield_function_t yield) mutable {
                        selection_sort(yield, vec_copy.begin(), vec_copy.end());
                        return vec_copy;
                    };

            return std::make_shared<AsyncWorker<decltype(lambda)>>(lambda);
        }

        throw std::logic_error("Unimplemented worker in random factory: " + worker_name);
    }
}

#endif //WORKERS_MANAGER_EXAMPLE_WORKERS_H
