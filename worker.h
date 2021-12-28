#ifndef WORKERS_MANAGER_WORKER_H
#define WORKERS_MANAGER_WORKER_H

#include <thread>
#include <iostream>
#include <atomic>
#include <condition_variable>
#include <optional>

/**
 * Abstract class for async workers implemented with std::thread_ that can be safely paused, restarted, stopped and destroyed.
 * Subclasses should implement work() method and regularly call status_update() method inside (see method docstrings for details).
 * It is assumed the Worker object is controlled from a single thread.
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
     * @throws std::logic_error if worker is not running when the method is called
     */
    void pause();

    /**
    * Restarts (resumes) worker (blocking call)
    * @throws std::logic_error if worker is not paused when the method is called
    */
    void restart();

    /**
    * Stops worker (blocking call). Worker can't be restarted after it is stopped
    * @throws std::logic_error if worker has already finished it's work
    */
    void stop();

protected:
    /**
     * Sleeps in case worker should be paused and checks if worker needs to stop.
     * Good worker implementations should should call this regularly (inside do_work method)
     * while still keeping in mind the overhead of this call (most notably the mutex lock)
     * @return boolean indicating whether the worker should cleanly stop (true) or keep running (false)
     */
    bool status_update();

    /**
     * Publishes worker's progress.
     * Good worker implementations should should call this regularly (inside do_work method).
     * @param progress worker's updated progress, in the 0-100 range (percentage)
     * @throws std::domain_error if progress passed is not in the 0-100 range
     */
    void set_progress(int progress);

private:
    /** Wrapper method that's to be run in separate thread */
    void work();

    /** Where the actual work that's to be run in separate thread is implemented. */
    virtual void do_work() = 0;

    int id_ = -1; // TODO: should it be part of the class? What's its use here?
    Status status_ = Status::RUNNING; // TODO: think about using atomic for this and status_change_ and relieving some of the locking
    std::atomic<int> progress_ = 0; // in percentages (0-100)

    std::thread thread_;
    std::optional<Status> status_change_; // scheduled status change
    mutable std::mutex status_m_; // mutex for accessing worker status
    mutable std::condition_variable status_cv_; // conditional variable for changing worker status
};

/** @throws std::domain_error if no string conversion for passed status */
std::ostream& operator<<(std::ostream& os, Worker::Status status);

std::ostream& operator<<(std::ostream& os, Worker& worker);

#endif //WORKERS_MANAGER_WORKER_H
