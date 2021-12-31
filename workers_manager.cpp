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
    po::options_description desc("Async workers manger. Options:");
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

void
parse_command(const std::vector<std::shared_ptr<worker::BaseWorker>>& workers, std::vector<std::string> command_split) {
    if (command_split.empty() || command_split[0].empty()) {
        return;
    }

    if (command_split[0] == "status") {
        if (command_split.size() != 1) {
            std::cout << "Invalid number of command tokens" << std::endl;
            return;
        }

        std::cout << "Workers status:" << std::endl;
        for (std::size_t i = 0; i < workers.size(); ++i) {
            std::cout << std::setw(5) << i + 1 << " | " << *workers[i] << std::endl;
        }
    }
    else {
        if (command_split.size() != 2) {
            std::cout << "Invalid number of command tokens" << std::endl;
            return;
        }

        try {
            int id = std::stoi(command_split[1]);
            if (id <= 0) {
                throw std::out_of_range("Negative or zero id");
            }

            auto& worker = workers.at(id - 1);

            if (command_split[0] == "pause") {
                worker->pause();
                std::cout << "Worker has been paused" << std::endl;
            }
            else if (command_split[0] == "restart") {
                worker->restart();
                std::cout << "Worker has been restarted" << std::endl;
            }
            else if (command_split[0] == "stop") {
                worker->stop();
                std::cout << "Worker has been stopped" << std::endl;
            }

        }
        catch (const std::invalid_argument& e) {
            std::cout << "First argument should be a number" << std::endl;
        }
        catch (const std::out_of_range& e) {
            std::cout << "Thread id should be in [1, " << workers.size() << "] range" << std::endl;
        }
        catch (const std::logic_error& e) {
            std::cout << "Error occured while processing command: " << e.what() << std::endl;
        }

    }
}

/**
 * CLI mainloop that parses commands from standard input and converts them to BaseWorker method calls
 * @param workers: vector of workers that will be managed
 * @param stop: atomic boolean used as a stopping condition
 */
void workers_manager(const std::vector<std::shared_ptr<worker::BaseWorker>> workers, std::atomic<bool>& stop) {
    // print help
    std::cout << "Welcome to Workers Manager" << std::endl;
    std::cout << "Commands: " << std::endl;
    std::cout << "  status - Prints id and status of all workers" << std::endl;
    std::cout << "  pause <id> - Pauses worker with id <id>" << std::endl;
    std::cout << "  restart <id> - Restarts (resumes) worker with id <id>" << std::endl;
    std::cout << "  stop <id> - Stops worker with id <id>" << std::endl;
    std::cout << std::string(40, '-') << std::endl;

    std::string command;
    std::vector<std::string> command_split;
    while (!stop) {
        std::cout << std::endl << "cmd: ";

        std::getline(std::cin, command);

        boost::split(command_split, command, boost::is_any_of(" \t"), boost::token_compress_on);
        parse_command(workers, command_split);
    }
}

int main(int argc, char** argv) {
    CmdOptions options = parse_cmd_options(argc, argv);

    // vector of random workers
    std::vector<std::shared_ptr<worker::BaseWorker>> workers(options.n_workers);
    std::generate(workers.begin(), workers.end(), &worker::random_worker);

    std::atomic<bool> stop_workers_manager = false;
    std::thread t(&workers_manager, workers, std::ref(stop_workers_manager));

    // wait for all workers to finish/stop
    for (auto& worker: workers) {
        worker->wait();
    }

    //TODO: Ctrl + C
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    stop_workers_manager = true;
    std::cout << "All workers stopped or finished. Press enter or Ctrl+C to quit..." << std::endl;

    t.join();
    return 0;
}
