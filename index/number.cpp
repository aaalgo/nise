#include <sstream>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "../common/nise.h"
#include "hadoop.h"

namespace po = boost::program_options; 

int main(int argc, char **argv) {
    int partitions;
    std::string input;
    std::string output;
    std::vector<std::string> conf;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("conf,c", po::value(&conf), "hadoop configuration formated as NAME=VALUE")
    ("partitions", po::value(&partitions)->default_value(1000), "")
    ("input", po::value(&input), "hadoop input")
    ("output", po::value(&output), "hadoop output")
    ;

    po::positional_options_description p;
    p.add("input", 1).add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0)) {
        std::cerr << "usage:" << std::endl;
        std::cerr << "\t" << argv[0] << " [--conf name=value ...] <hdfs://input> <hdfs://output>" << std::endl;
        std::cerr << desc;
        return 1;
    }

    nise::Environment env;
    std::string cmd = "hadoop jar " + env.jar_path() + " nise.Number " + input + " " + output + " "
                        + boost::lexical_cast<std::string>(partitions);
    system(cmd.c_str());

    return 0;
}

