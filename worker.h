#ifndef WORKERS_MANAGER_WORKER_H
#define WORKERS_MANAGER_WORKER_H

#include <iostream>
#include <atomic>
#include <future>
#include <utility>
#include <functional>
#include <condition_variable>
#include <optional>

//TODO: namespace

enum class WorkerStatus {
    RUNNING, PAUSED, STOPPED, FINISHED
};

// function type for yielding execution from worker function to Worker (see Worker::yield)
using yield_function_t = std::function<bool(int)>;

/**
 * Async worker that can be safely paused, restarted, stopped and destroyed.
 * Implemented by wrapping std::async - always run in separate thread.
 * @tparam Function function type (see std::async)
 * @tparam Args function arguments (see std::async)
 */
template<class Function, class... Args>
class Worker {
public:
    /**
     * Constructs worker from passed function & arguments (excluding the first argument - yield function)
     * and begins it's execution. The only requirement for passed functions is that it accepts yield function
     * as it's first argument. See yield member function docstring for details.
     */
    explicit Worker(Function&& f, Args&& ... args);

    // non-copyable
    Worker(const Worker& other) = delete;

    Worker& operator=(Worker& other) = delete;

    /** @return worker status (e.g. running, paused, ...) */
    [[nodiscard]] WorkerStatus status() const {
        std::lock_guard<std::mutex> lock(status_m_);
        return status_;
    }

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
    //TODO: function for obtaining result

private:
    // infer Function return type (notice the extra yield function argument that Function must accept)
    using function_return_t = std::invoke_result_t<std::decay_t<Function>, yield_function_t, std::decay_t<Args>...>;

    /** Wrapper worker method that's to be run in separate thread by std::async */
    function_return_t work(Function&& f, Args&& ... args);

    /** Called after worker function exits. Changes state to stopped or finished depending on the situation */
    void worker_fun_stopped();

    /**
     * Called by worker function (passed in constructor) to yield control of execution.
     * Sleeps in case worker should be paused and checks if worker needs to stop.
     * Also used to publish worker's progress.
     * Good worker functions should should call this regularly
     * while still keeping in mind the overhead of this call (most notably the mutex lock)
     * @param progress worker's updated progress, in the 0-100 range (percentage)
     * @throws std::domain_error if progress passed is not in the 0-100 range
     * @return boolean indicating whether the worker should cleanly stop (true) or keep running (false)
     */
    bool yield(int progress);

    /**
     * Set worker's progress
     * @param progress worker's updated progress, in the 0-100 range (percentage)
     * @throws std::domain_error if progress passed is not in the 0-100 range
     */
    void set_progress(int progress);

    WorkerStatus status_ = WorkerStatus::RUNNING; // TODO: think about using atomic for this and status_change_ and relieving some of the locking
    std::atomic<int> progress_ = 0; // in percentages (0-100)

    std::future<function_return_t> future_;

    std::optional<WorkerStatus> status_change_; // scheduled status change
    mutable std::mutex status_m_; // mutex for accessing worker status
    mutable std::condition_variable status_cv_; // conditional variable for changing worker status
};

/** @throws std::domain_error if no string conversion for passed status */
std::ostream& operator<<(std::ostream& os, WorkerStatus status);

template<class Function, class... Args>
std::ostream& operator<<(std::ostream& os, Worker<Function, Args...>& worker);


// ******* Template implementations ********************************************
template<class Function, class... Args>
Worker<Function, Args...>::Worker(Function&& f, Args&& ... args)  : future_(
        std::async(std::launch::async, &Worker::work, this, f, std::forward<Args>(args)...)) {}

template<class Function, class... Args>
void Worker<Function, Args...>::pause() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_ != WorkerStatus::RUNNING) {
        throw std::logic_error("Worker must be running to preform pause action");
    }

    status_change_ = WorkerStatus::PAUSED;

    // wait for pause to happen or for worker to finish
    status_cv_.wait(lock,
                    [this]() { return status_ == WorkerStatus::PAUSED || status_ == WorkerStatus::FINISHED; });
}

template<class Function, class... Args>
void Worker<Function, Args...>::restart() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_ != WorkerStatus::PAUSED) {
        throw std::logic_error("Worker must be paused to preform restart action");
    }

    status_change_ = WorkerStatus::RUNNING;
    status_cv_.notify_one();

    // wait for restart to happen or for worker to finish
    status_cv_.wait(lock,
                    [this]() { return status_ == WorkerStatus::RUNNING || status_ == WorkerStatus::FINISHED; });
}

template<class Function, class... Args>
void Worker<Function, Args...>::stop() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_ != WorkerStatus::RUNNING && status_ != WorkerStatus::PAUSED) {
        throw std::logic_error("Worker must be running or paused to preform stop action");
    }

    status_change_ = WorkerStatus::STOPPED;
    status_cv_.notify_one();
    lock.unlock();

    // wait for async task to finish
    future_.wait();
}

template<class Function, class... Args>
typename Worker<Function, Args...>::function_return_t Worker<Function, Args...>::work(Function&& f, Args&& ... args) {
    // yield function that's to be passed to worker function
    auto yield_func = std::bind(&Worker::yield, this, std::placeholders::_1);
    // TODO: there should be a better way
    // void return type needs to be handled separately
    if constexpr(std::is_same_v<function_return_t, void>) {
        f(yield_func, std::forward<Args>(args)...);
        worker_fun_stopped();
        return;
    }
    else {
        function_return_t ret = f(yield_func, std::forward<Args>(args)...);
        worker_fun_stopped();
        return ret;
    }
}

template<class Function, class... Args>
void Worker<Function, Args...>::worker_fun_stopped() {
    std::lock_guard<std::mutex> lock(status_m_);
    // worker could've finished or was stopped
    status_ = status_change_ != WorkerStatus::STOPPED ? WorkerStatus::FINISHED : WorkerStatus::STOPPED;

    // force 100% progress if worker finished
    if (status_ == WorkerStatus::FINISHED) {
        set_progress(100);
    }
}

template<class Function, class... Args>
bool Worker<Function, Args...>::yield(int progress) {
    set_progress(progress);

    std::unique_lock<std::mutex> lock(status_m_);
    if (status_change_ == WorkerStatus::PAUSED) {
        status_change_.reset();
        status_ = WorkerStatus::PAUSED;
        // notify of the status change
        status_cv_.notify_one();
        // sleep until restart or stop is requested
        status_cv_.wait(lock,
                        [this]() {
                            return status_change_ == WorkerStatus::RUNNING ||
                                   status_change_ == WorkerStatus::STOPPED;
                        });

        status_ = WorkerStatus::RUNNING;
        // notify of the wake
        status_cv_.notify_one();
    }

    if (status_change_ == WorkerStatus::STOPPED) {
        status_change_.reset();
        return false; // worker implementation needs to stop cleanly
    }

    return true;
}

template<class Function, class... Args>
void Worker<Function, Args...>::set_progress(int progress) {
    if (progress < 0 || progress > 100) {
        throw std::domain_error("Progress is outside the 0-100 range");
    }
    progress_ = progress;
}

std::ostream& operator<<(std::ostream& os, WorkerStatus status) {
    switch (status) {
        case WorkerStatus::RUNNING:
            return os << "running";
        case WorkerStatus::PAUSED:
            return os << "paused";
        case WorkerStatus::STOPPED:
            return os << "stopped";
        case WorkerStatus::FINISHED:
            return os << "finished";
    }

    throw std::domain_error("status does not have string conversion");
}

template<class Function, class... Args>
std::ostream& operator<<(std::ostream& os, Worker<Function, Args...>& worker) {
    auto worker_status = worker.status();
    os << "Worker" << " - " << worker_status;

    if (worker_status == WorkerStatus::RUNNING || worker_status == WorkerStatus::PAUSED) {
        os << "(" << worker.progress() << "% done)";
    }
    return os;
}

#endif //WORKERS_MANAGER_WORKER_H
