# Async worker

Single header library (`/include/worker/worker.hpp`) with `worker::AsyncWorker` class used to run async tasks that can be safely paused,
restarted and stopped. Implemented by wrapping `std::async` - but always run in separate thread.

## Dependecies
* C++17

# Examples
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
