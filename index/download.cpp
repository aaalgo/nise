#include <sstream>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "../common/nise.h"
#include "hadoop.h"

namespace po = boost::program_options; 

int main(int argc, char **argv) {
    std::string input;
    std::string output;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input", po::value(&input), "hadoop input dir")
    ("output", po::value(&output), "local output")
    ;

    po::positional_options_description p;
    p.add("input", 1).add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0)) {
        std::cerr << "usage:" << std::endl;
        std::cerr << "\t" << argv[0] << " <hdfs_input_dir> <local_output>" << std::endl;
        std::cerr << desc;
        return 1;
    }

    nise::Environment env;
    std::string cmd = "hadoop jar " + env.jar_path() + " nise.Download " + input + " " + output;
    system(cmd.c_str());

    return 0;
}

