#include "worker.h"

Worker::Worker(int id) : id_(id), thread_(&Worker::work, this) {}

Worker::~Worker() {
    try {
        stop();
    } catch (std::logic_error& e) { // worker has already finished, swallow
    } catch (std::system_error& e) { // mutex exception, don't propagate
    }
}

void Worker::pause() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_ != Status::RUNNING) {
        throw std::logic_error("Worker must be running to preform pause action");
    }

    status_change_ = Status::PAUSED;

    // wait for pause to happen or for worker to finish
    status_cv_.wait(lock, [this]() { return status_ == Status::PAUSED || status_ == Status::FINISHED; });
}

void Worker::restart() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_ != Status::PAUSED) {
        throw std::logic_error("Worker must be paused to preform restart action");
    }

    status_change_ = Status::RUNNING;
    status_cv_.notify_one();

    // wait for restart to happen or for worker to finish
    status_cv_.wait(lock, [this]() { return status_ == Status::RUNNING || status_ == Status::FINISHED; });
}

void Worker::stop() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_ != Status::RUNNING && status_ != Status::PAUSED) {
        throw std::logic_error("Worker must be running or paused to preform stop action");
    }

    status_change_ = Status::STOPPED;
    status_cv_.notify_one();
    lock.unlock();

    thread_.join();
}

bool Worker::status_update() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_change_ == Status::PAUSED) {
        status_ = Status::PAUSED;
        status_change_.reset();
        // notify of the status change
        status_cv_.notify_one();
        // sleep until restart or stop is requested
        status_cv_.wait(lock,
                        [this]() { return status_change_ == Status::RUNNING || status_change_ == Status::STOPPED; });
    }

    if (status_change_ == Status::STOPPED) {
        status_change_.reset();
        return false; // worker implementation needs to stop cleanly
    }

    return true;
}

void Worker::set_progress(int progress) {
    if (progress < 0 || progress > 100) {
        throw std::domain_error("Progress is outside the 0-100 range");
    }
    progress_ = progress;
}

void Worker::work() {
    do_work();

    std::lock_guard<std::mutex> lock(status_m_);
    // worker could've finished or was stopped
    status_ = status_change_ != Status::STOPPED ? Status::FINISHED : Status::STOPPED;
}

std::ostream& operator<<(std::ostream& os, Worker::Status status) {
    switch (status) {
        case Worker::Status::RUNNING:
            return os << "running";
        case Worker::Status::PAUSED:
            return os << "paused";
        case Worker::Status::STOPPED:
            return os << "stopped";
        case Worker::Status::FINISHED:
            return os << "finished";
    }

    throw std::domain_error("status does not have string conversion");
}

std::ostream& operator<<(std::ostream& os, Worker& worker) {
    auto worker_status = worker.status();
    os << "Worker " << worker.id() << " - " << worker_status;

    if (worker_status == Worker::Status::RUNNING || worker_status == Worker::Status::PAUSED) {
        os << "(" << worker.progress() << "% done)";
    }
    return os;
}