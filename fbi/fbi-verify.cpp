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
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "fbi.h"
using namespace std;
using namespace boost;
using namespace fbi;
namespace po = boost::program_options; 

int main(int argc, char **argv) {

    std::vector<Chunk> s1(DATA_CHUNK), s2(DATA_CHUNK);

    fill(s1.begin(), s1.end(), 0);

    ifstream is(argv[1], ios::binary);

    unsigned first = lexical_cast<unsigned>(argv[2]);
    unsigned i = 0;
    unsigned skip = lexical_cast<unsigned>(argv[3]);

    while (is.read((char *)&s2[0], s2.size() * sizeof(Chunk))) {
        if (Compare(&s1[0], &s2[0], first) > 0) {
            printf("%u\n", i);
            PrintPoint(&s1[0]);
            PrintPoint(&s2[0]);
        }
        if (skip) {
            is.seekg(skip, ios::cur);
        }
        i++;
        swap(s1, s2);
    }

    return 0;
}


