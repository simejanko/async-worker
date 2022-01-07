/** Examples of functions that can be used to create Worker instances and a random Worker factory function. **/

#ifndef WORKERS_MANAGER_EXAMPLE_WORKERS_HPP
#define WORKERS_MANAGER_EXAMPLE_WORKERS_HPP

#include <chrono>
#include <cmath>
#include <random>
#include <thread>
#include <sstream>

#include <worker/worker.hpp>

namespace worker {
    const std::vector<std::string> WORKER_EXAMPLES = {"dummy_worker", "fibonacci_slow", "selection_sort",
                                                      "file_writer"};

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
        auto distance = static_cast<double>(std::distance(first, last));

        auto n_sorted = 0;
        for (Iterator it = first; it != last; ++it) {
            std::iter_swap(it, std::min_element(it, last));
            ++n_sorted;

            if (!yield(n_sorted / distance)) {
                break;
            }
        }
    }

    /** Writes n_lines of length line_length to temporary file. */
    void file_writer(yield_function_t yield, int n_lines, int line_length) {
        const std::string ALPHABET = "abcdefghijklmnopqrstuvwxyz";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::size_t> alphabet_distr(0, ALPHABET.size() - 1);

        auto tmp_file = std::tmpfile();

        std::stringstream line;
        for (auto i = 0; i < n_lines; i++) {
            // generate random string
            for (auto j = 0; j < line_length; j++) {
                line << ALPHABET[alphabet_distr(gen)];
            }
            line << std::endl;

            std::fputs(line.str().c_str(), tmp_file);
            line.str(""); // clear stream

            // only yield execution every 100 lines
            if (i % 100 == 0 && !yield(static_cast<double>(i) / n_lines)) {
                break;
            }
        }

        std::fclose(tmp_file); // close & delete temporary file
    }

    /**
     * Factory function that returns random BaseWorker instances with random arguments, based on implementations in this file
     * @throws std::logic_error if worker that's not yet implemented in the factory is selected
     */
    std::shared_ptr<BaseWorker> random_worker() {
        // sample a random worker function from WORKER_EXAMPLES
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::size_t> distr(0, WORKER_EXAMPLES.size() - 1);
        std::string worker_name = WORKER_EXAMPLES[distr(gen)];

        if (worker_name == "dummy_worker") {
            std::uniform_int_distribution<int> loop_n_distr(200, 1000), sleep_ms_distr(10, 100);
            // unfortunately pointers and template deductions don't play nice
            return std::make_shared<AsyncWorker<decltype(&dummy_worker), int, int>>(
                    worker_name, &dummy_worker, loop_n_distr(gen), sleep_ms_distr(gen));
        }
        if (worker_name == "fibonacci_slow") {
            std::uniform_int_distribution<int> n_distr(35, 40);
            return std::make_shared<AsyncWorker<decltype(&fibonacci_slow), int>>(
                    worker_name, &fibonacci_slow, n_distr(gen));
        }
        if (worker_name == "selection_sort") {
            std::uniform_int_distribution<std::size_t> vec_size(20000, 150000);
            std::uniform_int_distribution<int> vec_distr(-1e5, 1e5);
            std::vector<int> rand_vec(vec_size(gen));
            std::generate(rand_vec.begin(), rand_vec.end(), [&]() { return vec_distr(gen); });

            // wrap selection sort with lambda that returns sorted copy
            auto lambda =
                    [vec_copy = rand_vec](yield_function_t yield) mutable {
                        selection_sort(yield, vec_copy.begin(), vec_copy.end());
                        return vec_copy;
                    };

            return std::make_shared<AsyncWorker<decltype(lambda)>>(worker_name, lambda);
        }

        if (worker_name == "file_writer") {
            std::uniform_int_distribution<int> n_lines_distr(1e5, 1e6);
            std::uniform_int_distribution<int> line_length_distr(50, 150);

            return std::make_shared<AsyncWorker<decltype(&file_writer), int, int>>(
                    worker_name, &file_writer, n_lines_distr(gen), line_length_distr(gen));
        }

        throw std::logic_error("Unimplemented worker in random factory: " + worker_name);
    }
}

#endif //WORKERS_MANAGER_EXAMPLE_WORKERS_HPP
