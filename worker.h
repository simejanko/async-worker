#ifndef WORKERS_MANAGER_WORKER_H
#define WORKERS_MANAGER_WORKER_H

#include <thread>
#include <iostream>

/**
 * Abstract class for async workers implemented with std::thread_ that can be safely paused, restarted, stopped and destroyed.
 */
class Worker {
public:
    enum class Status {
        RUNNING, PAUSED, STOPPED, FINISHED
    };

    /**
     * Constructs worker and begins it's execution
     * @param id number to identify this worker by
     * @param total_processing_steps number of high-level processing steps this worker consists of (used to track progress)
     */
    Worker(int id, int total_processing_steps);

    virtual ~Worker();

    // non-copyable
    Worker(const Worker& other) = delete;

    Worker& operator=(Worker& other) = delete;

    /** @return worker status (e.g. running, paused, ...) */
    [[nodiscard]] Status status() const { return status_; }

    /** @return number to identify this worker by */
    [[nodiscard]] int id() const { return id_; }

    /** @return worker's progress, in the 0-100 range (percentage) */
    [[nodiscard]] int progress() const;

    /**
     * Pauses worker
     * @throws std::logic_error if worker is not in running or paused state when the method is called
     */
    void pause();

    /**
    * Restarts (resumes) worker
    * @throws std::logic_error if worker is not in running or paused state when the method is called
    */
    void restart();

    /**
    * Stops worker. Worker can't be restarted after it is stopped.
    * @throws std::logic_error if worker has already finished it's work (finished state)
    */
    void stop();

private:
    /** Where the actual work that's to be run in separate thread is implemented. */
    virtual void work() = 0;

    int id_ = -1; // TODO: should it be part of the class? What's its use here?
    std::thread thread_;
    Status status_ = Status::RUNNING;

    // progress members
    int total_processing_steps_ = 1;
    int completed_steps_ = 0;

};

std::ostream& operator<<(std::ostream& os, Worker& worker);

#endif //WORKERS_MANAGER_WORKER_H
