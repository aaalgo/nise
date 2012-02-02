#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <boost/assert.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <boost/progress.hpp>
#include <boost/dynamic_bitset.hpp>
#include "../common/nise.h"

namespace po = boost::program_options; 

int main (int argc, char *argv[]) {
    unsigned total;
    std::string input;
    std::string output;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("total", po::value(&total), "")
    ("input", po::value(&input), "")
    ("output", po::value(&output), "")
    ;

    po::positional_options_description p;
    p.add("input", 1).add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0) || (vm.count("total") == 0)) {
        std::cerr << desc;
        return 1;
    }

    std::vector<std::vector<std::vector<nise::ImageID>>> groups(nise::CONTAINER_SIZE);
    std::vector<int32_t> ids(total);
    std::fill(ids.begin(), ids.end(), -1);
    nise::ImageID cur = 0;

    // loading, write whole containers
    std::ifstream is(input.c_str(), std::ios::binary);
    for (;;) {
        std::vector<nise::ImageID> gr;
        nise::ReadVector<nise::ImageID>(is, &gr);
        if (!is) break;
        if (gr.size() > nise::CONTAINER_SIZE) break; // reaching the end
        BOOST_VERIFY(gr.size() <= nise::CONTAINER_SIZE);
        if (gr.size() == nise::CONTAINER_SIZE) {
            BOOST_FOREACH(auto v, gr) {
                ids[v] = cur++;
            }
        }
        else {
            BOOST_FOREACH(auto v, gr) {
                ids[v] = -2;
            }
            std::vector<std::vector<nise::ImageID>> &t = groups[gr.size()];
            t.push_back(std::vector<nise::ImageID>());
            std::swap(gr, t.back());
        }
    }

    BOOST_VERIFY(groups[0].empty());
    BOOST_VERIFY(groups[1].empty());

    nise::ImageID sgl_off = 0;

    for (unsigned i = nise::CONTAINER_SIZE - 1; i > 1; --i) {
        while (groups[i].size()) {
            unsigned left = nise::CONTAINER_SIZE;
            unsigned j = i;
            while (j > 1) {
                if (groups[j].size()) {
                    BOOST_FOREACH(auto v, groups[j].back()) {
                        ids[v] = cur++;
                    }
                    groups[j].pop_back();
                    left -= j;
                    j = std::min(left, i);
                }
                else {
                    --j;
                }
            }
            while (left) {
                while ((ids[sgl_off] != -1) && (sgl_off < total)) sgl_off++;
                BOOST_VERIFY(sgl_off < total);
                ids[sgl_off++] = cur++;
                --left;
            }
        }
    }

    for (; sgl_off < total; ++sgl_off) {
        if (ids[sgl_off] < 0) {
            BOOST_VERIFY(ids[sgl_off] == -1);
            ids[sgl_off] = cur++;
        }
    }
    BOOST_VERIFY(cur == total);

    {
        std::ofstream os(output.c_str(), std::ios::binary);
        nise::WriteVector<int32_t>(os, ids);
    }

    return 0;
}

