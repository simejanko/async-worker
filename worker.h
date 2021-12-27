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
     */
    explicit Worker(int id);

    virtual ~Worker();

    // non-copyable
    Worker(const Worker& other) = delete;

    Worker& operator=(Worker& other) = delete;

    /** @return worker status (e.g. running, paused, ...) */
    [[nodiscard]] Status status() const { return status_; }

    /** @return number to identify this worker by */
    [[nodiscard]] int id() const { return id_; }

    /** @return worker's progress, in the 0-100 range (percentage) */
    [[nodiscard]] int progress() const { return progress_; };

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

protected:
    /**
     * Sets worker's progress, in the 0-100 range (percentage)
     * @throws std::domain_error if progress passed is not in the 0-100 range
     */
    void set_progress(int progress);

private:
    /** Where the actual work that's to be run in separate thread is implemented. */
    virtual void work() = 0;

    int id_ = -1; // TODO: should it be part of the class? What's its use here?
    std::thread thread_;
    Status status_ = Status::RUNNING;

    int progress_ = 0; // in percentages (0-100)

};

std::ostream& operator<<(std::ostream& os, Worker& worker);

#endif //WORKERS_MANAGER_WORKER_H
