#include "example_workers.h"

SlowFibonacci::SlowFibonacci(int n) : n_(n) {}

int SlowFibonacci::fib(int n) {
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return 1;
    }
    return fib(n - 1) + fib(n - 2);
}

void SlowFibonacci::do_work() {
    fib(n_);
}
