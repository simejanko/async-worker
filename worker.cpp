#include "worker.h"

Worker::Worker(int id, int total_processing_steps) : id_(id), total_processing_steps_(total_processing_steps) {}

Worker::~Worker() {

}

int Worker::progress() const {
    return 0;
}

void Worker::pause() {

}

void Worker::restart() {

}

void Worker::stop() {

}

std::ostream& operator<<(std::ostream& os, Worker& worker) {
    return os << worker.id();
}