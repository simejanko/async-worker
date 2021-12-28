/** Example implementations of Worker class */

#ifndef WORKERS_MANAGER_EXAMPLE_WORKERS_H
#define WORKERS_MANAGER_EXAMPLE_WORKERS_H

#include "worker.h"

/** Computes n-th Fibonacci number with a slow recursive algorithm*/
class SlowFibonacci : public Worker {
public:
    explicit SlowFibonacci(int n);

private:
    int fib(int n);

    void do_work() override;

    int n_; // fibonacci input
};

//TODO: more workers and factory function

#endif //WORKERS_MANAGER_EXAMPLE_WORKERS_H
