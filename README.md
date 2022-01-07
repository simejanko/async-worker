# Async worker

Single header library (`/include/worker/worker.hpp`) with `worker::AsyncWorker` class used to run async tasks that can be safely paused,
restarted and stopped. Implemented by wrapping `std::async` - but always run in separate thread.

## Dependecies
* C++17

# Examples
* Example of a trivial worker implementation with sleep & for loop
```C++
#include <worker/worker.hpp>

// all workers must accept worker::yield_function_t as first argument
std::string dummy_worker(worker::yield_function_t yield, int loop_n, int sleep_ms) {
    for (auto i = 0; i < loop_n; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

        auto progress = i / static_cast<double>(loop_n); // worker's progress in 0-1 range
        bool keep_running = yield(progress); // worker yields execution to report progress & pause if needed
        if (!keep_running) { // worker should cleanly stop
            return "stopped";
        }
    }
    return "finished";
}

int main() {
    worker::AsyncWorker dummy_async(&dummy_worker, 100, 10); // initializes & starts worker
    dummy_async.pause();
    dummy_async.restart();
    std::cout << "Worker result: " << dummy_async.result() << std::endl; // waits for result
}
```

* `example_workers.hpp`
  *  includes some example functions that can be wrapped with `worker::AsyncWorker`
  *  random worker factory function.

* `workers_manager.cpp` includes a simple CLI program that starts random workers and allows us to control them via standard input.
Depends on `Boost`.

# Workers Manager CLI
## Command line options
```
Workers Manager:
  --help                      prints help message
  -t [ --threads ] nb_threads number of worker threads to run (required)
```

## Standard Input CLI
```
Commands: 
  status - Prints id and status of all workers
  pause <id> - Pauses worker with id <id>
  restart <id> - Restarts (resumes) worker with id <id>
  stop <id> - Stops worker with id <id>
```

## Build
The CLI is built with `cmake`. Cmake needs to find `Boost` installation (tested on `1.78.0`) .
```
cd examples
mkdir build
cd build
cmake ..
make
```

## Prebuilt
* [Ubuntu 18.04 x86_64](https://drive.google.com/file/d/1aNx-UmNlZjtGMJNIBSGtVSRl1DwOtAFM/view?usp=sharing)
* [Windows 10 x86](https://drive.google.com/file/d/1wiWcLH8o-oqZ3uyatwloWPm_RekgBfdu/view?usp=sharing)
