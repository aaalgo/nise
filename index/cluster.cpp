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
#include "../common/nise.h"

class GraphNibble {

    // raw graph data
    std::vector<int> data;
    // all ids of graph vertices
    // will be shuffled for nibble
    std::vector<int> ids;

    // adjacent list
    // contains -1 entries for deleted vertices
    // count: # non -1 entries
    struct Neighbors {
        int *first;
        int count;
        unsigned size;
        Neighbors (): first(0), count(0), size(0) {}
    };

    // vertex to adjacent list mapping
    std::unordered_map<int, Neighbors> map;

    // remove vertices from graph
    void remove (const std::vector<int> vs) {
        BOOST_FOREACH(int v, vs) {
            Neighbors &nbr = map[v];
            BOOST_VERIFY(nbr.first); // ensure the vertex hasn't been removed
            int removed = 0;
            for (unsigned i = 0; i < nbr.size; ++i) {
                int n = nbr.first[i];
                if (n < 0) continue;
                Neighbors &nbr2 = map[n];
                for (unsigned j = 0; j < nbr2.size; ++j) {
                    if (nbr2.first[j] == v) {
                        nbr2.first[j] = -1;
                        ++removed;
                        --nbr2.count;
                    }
                }
                nbr.first[i] = -1;
                BOOST_VERIFY(nbr2.count >= 0);
            }
            BOOST_VERIFY(removed == nbr.count);
            nbr.first = 0;
            nbr.count = 0;
        }
    }

    // nibble parameters
    float alpha;
    float epsilon;
    unsigned maxit;

    // pagerank of one vertex
    // neighbor information is needed and
    // stored together
    struct Rank {
        Neighbors nbr;
        float p, r;
        Rank () : p(0), r(0) {
        }
    };

    typedef std::map<int, Rank> PageRank;

    // compute localized pagerank  starting from v
    void rank (int v, PageRank *ret)
    {
        PageRank pr;
        std::set<int> big; // { u | r(u)/d(u) > epsilon }

        {
            Rank &prv = pr[v];
            prv.nbr = map[v];
            BOOST_VERIFY(prv.nbr.first);
            BOOST_VERIFY(prv.nbr.count);
            prv.p = 0;
            prv.r = 1.0;
            if (prv.r / prv.nbr.count >= epsilon) big.insert(v);
        }

        unsigned it = 0;
        while ((!big.empty()) && (it < maxit)) {
            
            int u = *big.begin();

            Rank &pru = pr[u];

            pru.p += alpha * pru.r;
            pru.r *= (1 - alpha) / 2;

            float trans = pru.r / pru.nbr.count;

            if (trans < epsilon) {
                big.erase(u);
            }

            for (unsigned i = 0; i < pru.nbr.size; ++i) {
                int v = pru.nbr.first[i];
                if (v < 0) continue;
                Rank &prv = pr[v];
                if (prv.nbr.first == 0) {
                    prv.nbr = map[v];
                    BOOST_VERIFY(prv.nbr.first);
                    BOOST_VERIFY(prv.nbr.count);
                }
                prv.r += trans;
                if (prv.r / prv.nbr.count >= epsilon) big.insert(v);
            }
            ++it;
        }

        swap(pr, *ret);
        
    }

    void nibble (int seed, std::vector<int> *result) {
        result->clear();

        PageRank pr;
        rank(seed, &pr);

        if (pr.size() < nise::CONTAINER_SIZE) {
            BOOST_FOREACH(const PageRank::value_type &vt, pr) {
                result->push_back(vt.first);
            }
            return;
        }
        
        std::vector<std::pair<float, int> > sl;
        BOOST_FOREACH(const PageRank::value_type &vt, pr) {
            sl.push_back(std::make_pair(-vt.second.p / vt.second.nbr.count, vt.first));
        }
        std::sort(sl.begin(), sl.end());

        BOOST_VERIFY(sl[0].second == seed);

        for (unsigned i = 0; i < nise::CONTAINER_SIZE; ++i) {
            result->push_back(sl[i].second);
        }
    }

public:

    GraphNibble (const std::string &path, float alpha_, float epsilon_, unsigned maxit_)
        : alpha(alpha_), epsilon(epsilon_), maxit(maxit_) 
    {
        std::ifstream is(path.c_str(), std::ios::binary);
        BOOST_VERIFY(is);
        // determine the file size
        is.seekg(0, std::ios::end);
        size_t size = is.tellg();
        BOOST_VERIFY(size % sizeof(data[0]) == 0);
        data.resize(size / sizeof(data[0]));
        // load index
        is.seekg(0, std::ios::beg);
        is.read((char *)&data[0], size);
        BOOST_VERIFY(is);

            // count how many items are there
        unsigned cur = 0;
        while (cur < data.size()) {
            Neighbors info;
            int id = data[cur];
            unsigned len_pos = cur + 1;
            unsigned len = data[len_pos];
            info.first = &data[len_pos + 1];
            info.size = info.count = len;
            map[id] = info;
            cur += len + 2;
            ids.push_back(id);
        }
        std::random_shuffle(ids.begin(), ids.end());
    }

    void prune (unsigned limit) {
        std::vector<int> bad;
        BOOST_FOREACH(const auto &item, map) {
            if (item.second.count > limit) {
                bad.push_back(item.first);
            }
        }
        remove(bad);
    }

    void run (const std::string &output) {
        std::ofstream os(output.c_str(), std::ios::binary);
        boost::progress_display progress(ids.size(), std::cerr);
        BOOST_FOREACH(int v, ids) {
            Neighbors &nbr = map[v];
            if (nbr.first) {
                if (nbr.count == 0) {
                    // simply drop singletons
                    nbr.first = 0;
                }
                else {
                    std::vector<int> results;
                    nibble(v, &results);
                    remove(results);
                    nise::WriteVector<int>(os, results);
                }
            }
            ++progress;
        }
    }

    void verify () {
        BOOST_FOREACH(int v, ids) {
            Neighbors &nbr = map[v];
            BOOST_VERIFY(nbr.first); // ensure the vertex hasn't been removed
            for (unsigned i = 0; i < nbr.size; ++i) {
                int n = nbr.first[i];
                if (n < 0) continue;
                unsigned cnt = 0;
                Neighbors &nbr2 = map[n];
                for (unsigned j = 0; j < nbr2.size; ++j) {
                    if (nbr2.first[j] == v) {
                        ++cnt;
                    }
                }
                BOOST_VERIFY(cnt == 1);
            }
        }
    }

};

namespace po = boost::program_options; 

int main (int argc, char *argv[]) {
    std::string input;
    std::string output;
    unsigned limit;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("max", po::value(&limit)->default_value(1000), "")
    ("input", po::value(&input), "local input")
    ("output", po::value(&output), "local output")
    ;

    po::positional_options_description p;
    p.add("input", 1).add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0)) {
        std::cerr << desc;
        return 1;
    }

    GraphNibble graph(input, nise::NIBBLE_ALPHA, nise::NIBBLE_EPSILON, nise::NIBBLE_MAXIT);
    graph.prune(limit);
    graph.run(output);
    return 0;
}

