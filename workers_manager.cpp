/** CLI program that starts random workers and allows us to control them via standard input. */

#include <iostream>
#include <boost/program_options.hpp>

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
}


int main(int argc, char** argv) {
    auto options = parse_cmd_options(argc, argv);
    return 0;
}
