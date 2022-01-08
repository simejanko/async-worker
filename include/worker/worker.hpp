#ifndef WORKERS_MANAGER_WORKER_HPP
#define WORKERS_MANAGER_WORKER_HPP

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
     * Abstract base class for worker that can be paused, restarted and stopped.
     * Instances must be modified (paused, restarted, stopped) from a single thread.
     * Worker should be waited on or stopped before destruction, unless implementing class states otherwise.
     */
    class BaseWorker {
    public:
        BaseWorker() = default;

        /** @param name: Optional name for this worker. */
        explicit BaseWorker(std::string name) : name_(std::move(name)) {};

        /**
         * Pure virtual destructor declaration to mark an abstract class.
         * Worker isn't stopped or waited on in default implementation (noexcept).
         * If worker is not stopped/finished before destruction, std::terminate is called.
         */
        virtual ~BaseWorker() = 0;

        // non-copyable
        BaseWorker(const BaseWorker& other) = delete;

        BaseWorker& operator=(const BaseWorker& other) = delete;

        /** Returns worker name (can be empty). Thread-safe. */
        [[nodiscard]] std::string name() const noexcept { return name_; }

        /** Returns worker status (e.g. running, paused, ...). Thread-safe. */
        [[nodiscard]] Status status() const {
            std::lock_guard<std::mutex> lock(status_m_);
            return status_;
        }

        /** Returns worker's progress, in the 0-1 range (0%-100%). Thread-safe. */
        [[nodiscard]] double progress() const noexcept { return progress_; }

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

        /** Waits for worker to finish/stop. Thread-safe. */
        void wait() const;

    protected:
        /**
        * Must be called in a worker thread when the thread can yield control of execution.
        * Sleeps in case worker should be paused (until resumed) and checks if worker needs to stop.
        * Also used to publish worker's progress.
        * Good implementations should should call this regularly
        * while still keeping in mind the overhead of this call (most notably the mutex lock)
        * @param progress worker's updated progress, in the 0-1 range (0%-100%)
        * @return boolean indicating whether the worker should continue running (true) or cleanly stop (false)
        */
        [[nodiscard]] bool yield(double progress);

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
        /** Utility for checking terminal states (not thread safe) */
        bool terminal_status() const { return status_ == Status::STOPPED || status_ == Status::FINISHED; }

        const std::string name_;
        Status status_ = Status::RUNNING;
        std::atomic<double> progress_ = 0; // in percentages (0-1)

        std::optional<Status> status_change_; // scheduled status change
        mutable std::mutex status_m_; // mutex for accessing worker status
        mutable std::condition_variable status_cv_; // conditional variable for changing worker status
    };

    // function type for yielding execution from worker (see BaseWorker::yield)
    using yield_function_t = std::function<bool(double)>;

    /**
     * Async worker that can be paused, restarted, stopped and returns result.
     * Implemented by wrapping std::async - always run in separate thread.
     * Destructor will wait for worker to finish (see std::future destructor).
     * @tparam Function function type (see std::async). The main difference with std::async interface is
     *   that the function must accept yield function (yield_function_t) as it's first argument (see BaseWorker::yield).
     *   That is: function determines when it can yield execution by calling yield function inside it's own implementation.
     * @tparam Args function arguments (see std::async). Excludes the first mandatory argument - yield function.
     */
    template<class Function, class... Args>
    class AsyncWorker : public BaseWorker {
        // infer Function return type (notice the extra yield function argument that Function must accept)
        using function_return_t = std::invoke_result_t<std::decay_t<Function>, yield_function_t, std::decay_t<Args>...>;

    public:
        /** Constructs worker from passed function & arguments. */
        explicit AsyncWorker(Function f, Args... args) {
            start(std::move(f), std::move(args)...);
        }

        /** Constructs worker from passed function & arguments and optional name for this worker. */
        AsyncWorker(std::string name, Function f, Args... args) : BaseWorker(std::move(name)) {
            start(std::move(f), std::move(args)...);
        }

        /**
         * Returns worker's result. Blocks until the result is available (worker finished or stopped).
         * Note that the result might be invalid if worker was preemptively stopped (depends on worker implementation).
         * As this is wrapper for std::future::get, result can only be obtained once.
         * @throws std::future_error if future state is invalid (e.g. result already obtained)
         */
        function_return_t result() {
            if (!future_.valid()) { // explicitly checked & thrown since not all implementations throw exception
                throw std::future_error(std::future_errc::no_state);
            }
            return future_.get();
        }

    private:
        /** Runs std::async on work method. Called by constructors. */
        void start(Function&& f, Args&& ... args) {
            future_ = std::async(std::launch::async, &AsyncWorker::work, this, std::move(f), std::move(args)...);
        }

        /** Wrapper method that's run in separate thread by std::async */
        function_return_t work(Function&& f, Args&& ... args);

        std::future<function_return_t> future_;
    };

    /** @throws std::domain_error if no string conversion for passed status */
    std::ostream& operator<<(std::ostream& os, Status status);

    std::ostream& operator<<(std::ostream& os, const BaseWorker& worker);


    // ******* Implementations ********************************************
    BaseWorker::~BaseWorker() {
        if(!terminal_status()){
            std::terminate();
        }
    }

    void BaseWorker::pause() {
        std::unique_lock<std::mutex> lock(status_m_);
        if (status_ != Status::RUNNING) {
            throw std::logic_error("Worker must be running to preform pause action");
        }

        status_change_ = Status::PAUSED;

        // wait for pause to happen or for worker to finish/stop
        status_cv_.wait(lock, [this]() { return status_ == Status::PAUSED || terminal_status(); });
    }

    void BaseWorker::restart() {
        std::unique_lock<std::mutex> lock(status_m_);
        if (status_ != Status::PAUSED) {
            throw std::logic_error("Worker must be paused to preform restart action");
        }

        status_change_ = Status::RUNNING;
        // notify sleeping worker
        status_cv_.notify_all();

        // wait for restart to happen or for worker to finish/stop
        status_cv_.wait(lock, [this]() { return status_ == Status::RUNNING || terminal_status(); });
    }

    void BaseWorker::stop() {
        std::unique_lock<std::mutex> lock(status_m_);
        if (status_ != Status::RUNNING && status_ != Status::PAUSED) {
            throw std::logic_error("Worker must be running or paused to preform stop action");
        }

        status_change_ = Status::STOPPED;
        // notify potentially sleeping worker
        status_cv_.notify_all();

        // wait for worker to stop or finish
        status_cv_.wait(lock, [this]() { return terminal_status(); });
    }

    void BaseWorker::wait() const {
        std::unique_lock<std::mutex> lock(status_m_);
        // check if already finished
        if (status_ == Status::STOPPED || status_ == Status::FINISHED) {
            return;
        }

        status_cv_.wait(lock, [this]() { return terminal_status(); });
    }

    bool BaseWorker::yield(double progress) {
        set_progress(progress);

        std::unique_lock<std::mutex> lock(status_m_);
        if (status_change_ == Status::PAUSED) {
            status_change_.reset();
            status_ = Status::PAUSED;
            // notify of the status change
            status_cv_.notify_all();
            // sleep until restart or stop is requested
            status_cv_.wait(lock, [this]() {
                return status_change_ == Status::RUNNING || status_change_ == Status::STOPPED;
            });

            status_ = Status::RUNNING;
            // notify of the wake
            status_cv_.notify_all();
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
        // notify of the status change
        status_cv_.notify_all();
    }

    template<class Function, class... Args>
    typename AsyncWorker<Function, Args...>::function_return_t
    AsyncWorker<Function, Args...>::work(Function&& f, Args&& ... args) {
        // yield function that's to be passed to worker function
        auto yield_func = std::bind(&AsyncWorker::yield, this, std::placeholders::_1);

        // void return type needs to be handled separately
        if constexpr(std::is_same_v<function_return_t, void>) {
            f(yield_func, std::move(args)...);
            worker_done();
            return;
        }
        else {
            function_return_t ret = f(yield_func, std::move(args)...);
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

    std::ostream& operator<<(std::ostream& os, const BaseWorker& worker) {
        auto worker_status = worker.status();
        os << "worker " << std::setw(20) << worker.name() << " - " << std::setw(10) << worker_status;

        if ((worker_status == Status::RUNNING || worker_status == Status::PAUSED) && worker.progress() > 0) {
            os << " (" << std::setw(3) << std::round(worker.progress() * 100) << "% done)";
        }
        return os;
    }
}
#endif //WORKERS_MANAGER_WORKER_HPP