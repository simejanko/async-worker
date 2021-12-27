#ifndef WORKERS_MANAGER_WORKER_H
#define WORKERS_MANAGER_WORKER_H

#include <thread>
#include <iostream>
#include <atomic>
#include <condition_variable>
#include <optional>

/**
 * Abstract class for async workers implemented with std::thread_ that can be safely paused, restarted, stopped and destroyed.
 * Subclasses should implement work() method and regularly call update() method inside (see method docstrings for details).
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
    [[nodiscard]] Status status() const {
        std::lock_guard<std::mutex> lock(status_m_);
        return status_;
    }

    /** @return number to identify this worker by */
    [[nodiscard]] int id() const { return id_; }

    /** @return worker's progress, in the 0-100 range (percentage) */
    [[nodiscard]] int progress() const { return progress_; };

    /**
     * Pauses worker (blocking call)
     * @throws std::logic_error if worker is not in running or paused state when the method is called
     */
    void pause();

    /**
    * Restarts (resumes) worker (blocking call)
    * @throws std::logic_error if worker is not in running or paused state when the method is called
    */
    void restart();

    /**
    * Stops worker (blocking call). Worker can't be restarted after it is stopped
    * @throws std::logic_error if worker has already finished it's work (finished state)
    */
    void stop();

protected:
    /**
     * Publishes worker's progress, sleeps in case worker should be paused and checks if worker needs to stop.
     * Good worker implementations should should call this regularly (inside work method)
     * while still keeping in mind the overhead of this call
     * @param progress worker's updated progress, in the 0-100 range (percentage)
     * @return boolean indicating whether the worker should cleanly stop (true) or keep running (false)
     * @throws std::domain_error if progress passed is not in the 0-100 range
     */
    bool update(int progress);

private:
    /** Where the actual work that's to be run in separate thread is implemented. */
    virtual void work() = 0;

    int id_ = -1; // TODO: should it be part of the class? What's its use here?
    Status status_ = Status::RUNNING;
    std::atomic<int> progress_ = 0; // in percentages (0-100)

    std::thread thread_; // worker thread
    std::optional<Status> status_change_; // scheduled status change
    mutable std::mutex status_m_; // mutex for accessing worker status
    mutable std::condition_variable status_cv_; // conditional variable for changing worker status
};

std::ostream& operator<<(std::ostream& os, Worker& worker);

#endif //WORKERS_MANAGER_WORKER_H
