// This program tries to find matches between local
// interesting points from two images using
// various methods.

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <limits>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include "fbi.h"

using namespace std;
namespace po = boost::program_options; 
using namespace fbi;

int main(int argc, char **argv) {

    string input_path;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input,I", po::value(&input_path), "")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0)) {
        cout << desc;
        return 1;
    }

    boost::interprocess::file_mapping m_file(input_path.c_str(), boost::interprocess::read_write);
    boost::interprocess::mapped_region region(m_file, boost::interprocess::read_write);
            
    Point *ptr = (Point *)region.get_address();
    Point *ptr_end = (Point *)((char *)region.get_address() + region.get_size());

    BOOST_VERIFY(region.get_size() % RECORD_SIZE == 0);

    cout << ptr << ' ' << ptr_end << endl;
    random_shuffle(ptr, ptr_end);

    return 0;
}


