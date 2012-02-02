// This program tries to find matches between local
// interesting points from two images using
// various methods.

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include "../common/nise.h"
#include "../image/extractor.h"

using namespace std;
using namespace nise;
namespace po = boost::program_options; 

int main(int argc, char **argv) {

    bool input_list = false;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("list", "read a list of paths from stdin")
    ;

    po::positional_options_description p;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help")) {
        cerr << desc;
        return 1;
    }

    if (vm.count("list")) {
        input_list = true;
    }

    Extractor xtor;

    for (;;) {
        std::string binary;

        if (input_list) {
            std::string path;
            cin >> path;
            if (!cin) break;
            nise::ReadFile(path, &binary);
        }
        else {
            nise::Signature::IMAGE.check(cin);
            if (!cin) break;
            nise::ReadString(cin, &binary);
            if (!cin) break;
        }

        nise::Record record;

        xtor.extract(binary, &record);

        nise::Signature::RECORD.write(cout);
        record.write(cout);
    }
    return 0;
}

