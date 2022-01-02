/** CLI program that starts random workers and allows us to control them via standard input. */

#include <iostream>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include "example_workers.h"

/** Command line options */
struct CmdOptions {
    int n_workers{};
};

/**
 * Parses command line options using boost::program_options.
 * Exits the program in case of failure or if only help message should be displayed.
 */
CmdOptions parse_cmd_options(int argc, char** argv) {
    namespace po = boost::program_options;

    CmdOptions options;

    // Declare the supported options.
    po::options_description desc("Workers Manager");
    desc.add_options()
            ("help", "prints help message")
            ("threads,t", po::value<int>(&options.n_workers)->required()->value_name("nb_threads"),
             "number of worker threads to run (required)");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    }
    catch (const po::error& e) {
        std::cerr << "Error parsing command line options: " << e.what() << std::endl;
        std::exit(2);
    }

    // print help (checked before po::notify to prevent preemptive missing required parameters errors)
    if (vm.count("help") || vm.empty()) {
        std::cout << desc << std::endl;
        std::exit(1);
    }

    try {
        po::notify(vm);
    }
    catch (const po::error& e) {
        std::cerr << "Error parsing command line options: " << e.what() << std::endl;
        std::exit(2);
    }


    // validation
    if (options.n_workers <= 0) {
        std::cerr << "Number of threads should be a positive integer (is " << options.n_workers << ")";
        std::exit(2);
    }

    return options;
}

/**
 * Accepts commands for controlling workers from standard input and executes them.
 * It's mainloop may be started from a different thread.
 */
class WorkersManagerCLI {
public:
    /** Accepts vector of BaseWorker instances to manage */
    explicit WorkersManagerCLI(std::vector<std::shared_ptr<worker::BaseWorker>> workers) :
            workers_(std::move(workers)) {}

    /** Reads commands from standard input & executes them until stopped. */
    void mainloop() {
        print_help();

        std::string command;
        std::vector<std::string> tokenized_comand;

        while (!stop_) {
            std::cout << std::endl << "cmd: ";

            std::getline(std::cin, command);

            boost::split(tokenized_comand, command, boost::is_any_of(" \t"), boost::token_compress_on);
            execute_command(tokenized_comand);
        }
    }

    void stop() {
        stop_ = true;
        std::cout << "Workers Manager stopped. Press enter to quit..." << std::endl;
    }

private:
    /** Prints help message with available commands */
    static void print_help() {
        std::cout << "Welcome to Workers Manager" << std::endl;
        std::cout << "Commands: " << std::endl;
        std::cout << "  status - Prints id and status of all workers" << std::endl;
        std::cout << "  pause <id> - Pauses worker with id <id>" << std::endl;
        std::cout << "  restart <id> - Restarts (resumes) worker with id <id>" << std::endl;
        std::cout << "  stop <id> - Stops worker with id <id>" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
    }

    /**
     * Parses and executes a single command
     * @param tokenized_comand command, that's already been tokenized into words
     */
    void execute_command(std::vector<std::string>& tokenized_comand) {
        if (tokenized_comand.empty() || tokenized_comand[0].empty()) { // nothing to parse
            return;
        }

        std::string main_command = tokenized_comand[0];

        if (tokenized_comand.size() == 1) { // commands without arguments
            if (main_command == "status") {
                std::cout << "Workers status:" << std::endl;
                for (std::size_t i = 0; i < workers_.size(); ++i) {
                    std::cout << std::setw(5) << i + 1 << " | " << *workers_[i] << std::endl;
                }
                return;
            }
        }
        else if (tokenized_comand.size() == 2) { // assume commands with a single worker id argument
            try {
                int id = std::stoi(tokenized_comand[1]);
                if (id <= 0) {
                    throw std::out_of_range("Negative or zero id");
                }

                auto& worker = workers_.at(id - 1); // ids start with 1

                if (main_command == "pause") {
                    worker->pause();
                    std::cout << "Worker has been paused" << std::endl;
                    return;
                }
                else if (main_command == "restart") {
                    worker->restart();
                    std::cout << "Worker has been restarted" << std::endl;
                    return;
                }
                else if (main_command == "stop") {
                    worker->stop();
                    std::cout << "Worker has been stopped" << std::endl;
                    return;
                }
            }
            catch (const std::invalid_argument&) {
                std::cout << "Second argument should be a number" << std::endl;
                return;
            }
            catch (const std::out_of_range&) {
                std::cout << "Worker id should be in [1, " << workers_.size() << "] range" << std::endl;
                return;
            }
            catch (const std::logic_error& e) {
                std::cout << "Error occurred while processing command: " << e.what() << std::endl;
                return;
            }
        }
        std::cout << "Unrecognized command format" << std::endl;
    }

    std::atomic<bool> stop_ = false;
    std::vector<std::shared_ptr<worker::BaseWorker>> workers_;
};

int main(int argc, char** argv) {
    CmdOptions options = parse_cmd_options(argc, argv);

    // vector of random workers
    std::vector<std::shared_ptr<worker::BaseWorker>> workers(options.n_workers);
    std::generate(workers.begin(), workers.end(), &worker::random_worker);

    // run worker manager cli in a separate thread
    WorkersManagerCLI workers_manager(workers);
    std::thread worker_manager_thread(&WorkersManagerCLI::mainloop, &workers_manager);

    // wait for all workers to finish/stop
    for (auto& worker: workers) {
        worker->wait();
    }
    std::cout << std::endl << "All workers stopped or finished" << std::endl;

    // finally, wait for workers manager CLI to stop
    workers_manager.stop();
    worker_manager_thread.join();
    return 0;
}
