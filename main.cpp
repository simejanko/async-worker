#include <iostream>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char** argv) {
    int n_workers;

    // Declare the supported options.
    po::options_description desc("Options");
    desc.add_options()
            ("help", "prints help message")
            ("threads,t", po::value<int>(&n_workers)->required()->value_name("nb_threads"),
             "number of worker threads to run (required)");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    }
    catch (const po::error& e) {
        std::cerr << "Error parsing command line options: " << e.what() << std::endl;
        std::cout << desc << std::endl; // print help as well
        return 2;
    }

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const po::error& e) {
        std::cerr << "Error parsing command line options: " << e.what() << std::endl;
        std::cout << desc << std::endl; // print help as well
        return 2;
    }

    if (n_workers <= 0) {
        std::cerr << "Number of threads should be a positive integer (is " << n_workers << ")";
    }
}
