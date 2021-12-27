#include "worker.h"

Worker::Worker(int id) : id_(id) {}

Worker::~Worker() {

}

void Worker::pause() {

}

void Worker::restart() {

}

void Worker::stop() {

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