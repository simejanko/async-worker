#ifndef WORKERS_MANAGER_WORKER_H
#define WORKERS_MANAGER_WORKER_H

#include <iostream>
#include <atomic>
#include <future>
#include <utility>
#include <functional>
#include <condition_variable>
#include <optional>
#include <iomanip>

namespace worker {
    enum class Status {
        RUNNING, PAUSED, STOPPED, FINISHED
    };

    /**
     * Abstract base async worker class that can be paused, restarted, stopped and destroyed.
     * It's instances must be controlled (e.g. paused, restarted) from a single thread
     */
    class BaseWorker {
    public:
        BaseWorker() = default; // explicit default since copy-constructor is explicitly deleted
        virtual ~BaseWorker() = default;

        // non-copyable
        BaseWorker(const BaseWorker& other) = delete;

        BaseWorker& operator=(BaseWorker& other) = delete;

        /** Returns worker status (e.g. running, paused, ...) */
        [[nodiscard]] Status status() const {
            std::lock_guard<std::mutex> lock(status_m_);
            return status_;
        }

        /** Returns worker's progress, in the 0-1 range (0%-100%) */
        [[nodiscard]] double progress() const { return progress_; };

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

        /** Waits for worker to finish/stop. */
        virtual void wait() = 0;

    protected:
        /**
        * Must be called in a worker thread when it can yield control of execution.
        * Sleeps in case worker should be paused and checks if worker needs to stop.
        * Also used to publish worker's progress.
        * Good implementations should should call this regularly
        * while still keeping in mind the overhead of this call (most notably the mutex lock)
        * @param progress worker's updated progress, in the 0-1 range (0%-100%)
        * @return boolean indicating whether the worker should cleanly stop (true) or keep running (false)
        */
        bool yield(double progress);

        /**
         * Set worker's progress. Clamped to the valid range.
         * @param progress worker's updated progress, in the 0-1 range (0%-100%)
         */
        void set_progress(double progress) { progress_ = std::clamp(progress, 0., 1.); }

        /**
         * Needs to be called by implementations when worker is done.
         * Changes state to stopped or finished depending on the type of exit.
         */
        void worker_done();

    private:
        Status status_ = Status::RUNNING;
        std::atomic<double> progress_ = 0; // in percentages (0-1)

        std::optional<Status> status_change_; // scheduled status change
        mutable std::mutex status_m_; // mutex for accessing worker status
        mutable std::condition_variable status_cv_; // conditional variable for changing worker status
    };

    // function type for yielding execution from worker (see BaseWorker::yield)
    using yield_function_t = std::function<bool(double)>;

    /**
     * Async worker that can be paused, restarted, stopped, destroyed and returns result.
     * Implemented by wrapping std::async - always run in separate thread.
     * @tparam Function function type (see std::async)
     * @tparam Args function arguments (see std::async)
     */
    template<class Function, class... Args>
    class Worker : public BaseWorker {
        // infer Function return type (notice the extra yield function argument that Function must accept)
        using function_return_t = std::invoke_result_t<std::decay_t<Function>, yield_function_t, std::decay_t<Args>...>;

    public:
        /**
         * Constructs worker from passed function & arguments (excluding the first argument - yield function)
         * and begins it's execution. The only requirement for passed functions is that it accepts yield function
         * as it's first argument. See yield member function docstring for details.
         */
        explicit Worker(Function&& f, Args&& ... args);

        /** Returns worker's result. Blocks until the result is available (worker finished or stopped).
         * As this is wrapper for std::future::get, result can only be obtained once.
         * @throws std::future_error if future state is invalid (e.g. result already obtained)
         */
        function_return_t result() {
            if (!future_.valid()) { // explicitly checked & thrown since not all implementations throw exception
                throw std::future_error(std::future_errc::no_state);
            }
            return future_.get();
        }

        /** Waits for worker to finish/stop.
         * As this is wrapper for std::future::wait, it shouldn't be called after worker's result has been obtained.
         * @throws std::future_error if future state is invalid (e.g. result already obtained)
         */
        void wait() override {
            if (!future_.valid()) { // explicitly checked & thrown since not all implementations throw exception
                throw std::future_error(std::future_errc::no_state);
            }
            future_.wait();
        }

    private:
        /** Wrapper method that's to be run in separate thread by std::async */
        function_return_t work(Function&& f, Args&& ... args);

        std::future<function_return_t> future_;
    };

    /** @throws std::domain_error if no string conversion for passed status */
    std::ostream& operator<<(std::ostream& os, Status status);

    std::ostream& operator<<(std::ostream& os, BaseWorker& worker);


    // ******* Implementations ********************************************
    void BaseWorker::pause() {
        std::unique_lock<std::mutex> lock(status_m_);
        if (status_ != Status::RUNNING) {
            throw std::logic_error("Worker must be running to preform pause action");
        }

        status_change_ = Status::PAUSED;

        // wait for pause to happen or for worker to finish
        status_cv_.wait(lock,
                        [this]() { return status_ == Status::PAUSED || status_ == Status::FINISHED; });
    }

    void BaseWorker::restart() {
        std::unique_lock<std::mutex> lock(status_m_);
        if (status_ != Status::PAUSED) {
            throw std::logic_error("Worker must be paused to preform restart action");
        }

        status_change_ = Status::RUNNING;
        status_cv_.notify_one();

        // wait for restart to happen or for worker to finish
        status_cv_.wait(lock,
                        [this]() { return status_ == Status::RUNNING || status_ == Status::FINISHED; });
    }

    void BaseWorker::stop() {
        std::unique_lock<std::mutex> lock(status_m_);
        if (status_ != Status::RUNNING && status_ != Status::PAUSED) {
            throw std::logic_error("Worker must be running or paused to preform stop action");
        }

        status_change_ = Status::STOPPED;
        status_cv_.notify_one();
        lock.unlock();

        // wait for worker to finish/stop
        wait();
    }

    bool BaseWorker::yield(double progress) {
        set_progress(progress);

        std::unique_lock<std::mutex> lock(status_m_);
        if (status_change_ == Status::PAUSED) {
            status_change_.reset();
            status_ = Status::PAUSED;
            // notify of the status change
            status_cv_.notify_one();
            // sleep until restart or stop is requested
            status_cv_.wait(lock, [this]() {
                return status_change_ == Status::RUNNING || status_change_ == Status::STOPPED;
            });

            status_ = Status::RUNNING;
            // notify of the wake
            status_cv_.notify_one();
        }

        if (status_change_ == Status::STOPPED) {
            return false; // worker implementation needs to stop cleanly
        }

        return true;
    }

    void BaseWorker::worker_done() {
        std::lock_guard<std::mutex> lock(status_m_);
        // worker could've finished or was stopped
        status_ = status_change_ != Status::STOPPED ? Status::FINISHED : Status::STOPPED;

        // force 100% progress if worker finished
        if (status_ == Status::FINISHED) {
            set_progress(1);
        }
    }

    template<class Function, class... Args>
    Worker<Function, Args...>::Worker(Function&& f, Args&& ... args)  : future_(
            std::async(std::launch::async, &Worker::work, this, f, std::forward<Args>(args)...)) {}

    template<class Function, class... Args>
    typename Worker<Function, Args...>::function_return_t
    Worker<Function, Args...>::work(Function&& f, Args&& ... args) {
        // yield function that's to be passed to worker function
        auto yield_func = std::bind(&Worker::yield, this, std::placeholders::_1);

        // void return type needs to be handled separately
        if constexpr(std::is_same_v<function_return_t, void>) {
            f(yield_func, std::forward<Args>(args)...);
            worker_done();
            return;
        }
        else {
            function_return_t ret = f(yield_func, std::forward<Args>(args)...);
            worker_done();
            return ret;
        }
    }

    std::ostream& operator<<(std::ostream& os, Status status) {
        switch (status) {
            case Status::RUNNING:
                return os << "running";
            case Status::PAUSED:
                return os << "paused";
            case Status::STOPPED:
                return os << "stopped";
            case Status::FINISHED:
                return os << "finished";
        }

        throw std::domain_error("status does not have string conversion");
    }

    std::ostream& operator<<(std::ostream& os, BaseWorker& worker) {
        auto worker_status = worker.status();
        os << "worker" << " - " << worker_status;

        if (worker_status == Status::RUNNING || worker_status == Status::PAUSED) {
            os << " (" << std::round(worker.progress() * 100) << "% done)";
        }
        return os;
    }
}
#endif //WORKERS_MANAGER_WORKER_H