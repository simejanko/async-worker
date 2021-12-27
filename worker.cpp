#include "worker.h"

Worker::Worker(int id) : id_(id) {}

Worker::~Worker() {

}

void Worker::pause() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_ == Status::PAUSED) {
        return;
    }

    if (status_ != Status::RUNNING) {
        throw std::logic_error("Worker must be running or paused.");
    }

    status_change_ = Status::PAUSED;

    // wait for pause to happen
    status_cv_.wait(lock, [this]() { return status_ == Status::PAUSED; });
}

void Worker::restart() {
    std::unique_lock<std::mutex> lock(status_m_);
    if (status_ == Status::RUNNING) {
        return;
    }

    if (status_ != Status::PAUSED) {
        throw std::logic_error("Worker is not paused or running.");
    }

    status_change_ = Status::RUNNING;
    status_cv_.notify_one(); //TODO: check if this is ok, without manually unlocking prior

    // wait for restart to happen
    status_cv_.wait(lock, [this]() { return status_ == Status::RUNNING; });
}

void Worker::stop() {
}

bool Worker::update(int progress) {
    if (progress < 0 || progress > 100) {
        throw std::domain_error("Progress is outside the 0-100 range");
    }
    progress_ = progress;

    //TODO: handle status change, waking from paused, etc...

    return false;
}

std::ostream& operator<<(std::ostream& os, Worker& worker) {
    return os << worker.id();
}