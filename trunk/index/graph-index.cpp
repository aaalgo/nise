#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <boost/assert.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <boost/progress.hpp>
#include <boost/dynamic_bitset.hpp>
#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/Process.h>
#include "../common/nise.h"

namespace po = boost::program_options; 

int main (int argc, char *argv[]) {
    std::string input;
    std::string mapping;
    std::string output;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input", po::value(&input), "")
    ("map", po::value(&mapping), "")
    ("output", po::value(&output), "")
    ;

    po::positional_options_description p;
    p.add("input", 1).add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0) || (vm.count("map") == 0)) {
        std::cerr << desc;
        return 1;
    }

    std::vector<nise::ImageID> map;

    {
        std::ifstream is(mapping.c_str(), std::ios::binary);
        nise::ReadVector<nise::ImageID>(is, &map);
    }

    // loading, write whole containers
    std::ifstream is(input.c_str(), std::ios::binary);
    std::ofstream os(output.c_str(), std::ios::binary);
    for (;;) {
        nise::ImageID id;
        std::vector<nise::ImageID> gr;
        id = nise::ReadUint32(is);
        nise::ReadVector<nise::ImageID>(is, &gr);
        if (!is) break;
        nise::WriteUint32(os, map[id]);
        BOOST_FOREACH(auto &v, gr) {
            v = map[v];
        }
        nise::WriteVector<nise::ImageID>(os, gr);
    }

    return 0;
}

