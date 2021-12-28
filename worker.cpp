#include "worker.h"

Worker::Worker(int id) : id_(id) {}

Worker::~Worker() {
    try {
        stop();
    } catch (std::logic_error& e) { // worker has already finished, swallow
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
    status_cv_.notify_one(); //TODO: check if this is ok, without manually unlocking prior

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

std::ostream& operator<<(std::ostream& os, Worker& worker) {
    return os << worker.id();
}