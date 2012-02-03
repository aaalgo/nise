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

using namespace std;
namespace po = boost::program_options; 

int main(int argc, char **argv) {

    string input_path;
    string output_path;
    unsigned data_size;
    unsigned key_size;
    unsigned sample_rate;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input,I", po::value(&input_path), "")
    ("output,O", po::value(&output_path), "")
    (",D", po::value(&data_size)->default_value(128/8), "")
    (",K", po::value(&key_size)->default_value(4), "")
    (",S", po::value(&sample_rate)->default_value(1000), "")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0)) {
        cout << desc;
        return 1;
    }

    unsigned record_size = data_size + key_size;
    unsigned skip_size = record_size * (sample_rate - 1);

    ifstream is(input_path.c_str(), ios::binary);
    ofstream os(output_path.c_str(), ios::binary);

    vector<char> buf(record_size);

    while(is.read(&buf[0], record_size)) {
        os.write(&buf[0], data_size);
        is.seekg(skip_size, ios::cur);
    }

    os.close();
    is.close();



    return 0;
}


