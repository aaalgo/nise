#ifndef WDONG_NISE_EXPAND
#define WDONG_NISE_EXPAND
#include <set>
#include <fstream>
#include <algorithm>
#include <map>
#include <vector>
#include <cmath>
#include <limits>
#include <boost/assert.hpp>
#include <boost/foreach.hpp>
#include <Poco/SharedMemory.h>

namespace nise {

    class Graph {
        ImageID *data;
        std::map<ImageID, unsigned> map;
        Poco::SharedPtr<Poco::SharedMemory> shared;
    public:
        typedef std::pair<const ImageID *, const ImageID *> Range;

        Graph (const std::string &path)
        {
            data = 0;
            if (!path.empty()) {
                shared = new Poco::SharedMemory(Poco::File(path), Poco::SharedMemory::AM_READ);
                size_t size = shared->end() - shared->begin();
                BOOST_VERIFY(size % sizeof(data[0]) == 0);
                size /= sizeof(data[0]);

                data = reinterpret_cast<ImageID *>(shared->begin());

                // count how many items are there
                unsigned cur = 0;
                while (cur < size) {
                    ImageID id = data[cur];
                    unsigned len_pos = cur + 1;
                    unsigned len = data[len_pos];
                    map[id] = len_pos;
                    cur += len + 2;
                }
            }
        }

        unsigned size () const {
            return map.size();
        }

        unsigned degree (ImageID v) const {
            std::map<ImageID, unsigned>::const_iterator it
                = map.find(v);
            if (it == map.end()) return 0;
            return data[it->second];
        }

        Range get (ImageID id) const {
            Range result;
            result.first = result.second = &data[0];
            std::map<ImageID, unsigned>::const_iterator it
                = map.find(id);
            if (it == map.end()) return result;
            unsigned idx = it->second;
            result.first = &data[idx+1];
            result.second = result.first + data[idx];
            return result;
        }
    };

    class Nibble {
        const Graph &g;
        float alpha;
        float epsilon;
        unsigned maxit;

        struct Info {
            float p, r;
            Info (): p(0), r(0) {}
        };

        typedef std::map<ImageID, Info> PageRank;

        void rank (std::vector<ImageID> vs, PageRank *ret)
        {
            PageRank pr;
            std::set<ImageID> big; // { u | r(u)/d(u) > epsilon }

            BOOST_FOREACH(ImageID v, vs) {
                Info &prv = pr[v];
                prv.p = 0;
                prv.r = 1.0F / vs.size();
                if (prv.r / g.degree(v) >= epsilon) big.insert(v);
            }

            unsigned it = 0;
            while (!big.empty()) {
                
                if (it >= maxit) {
                    break;
                }
                ImageID u = *big.begin();

                Info &pru = pr[u];

                pru.p += alpha * pru.r;
                pru.r *= (1 - alpha) / 2;

                float trans = pru.r / g.degree(u);

                if (trans < epsilon) {
                    big.erase(u);
                }

                BOOST_FOREACH(ImageID v, g.get(u)) {
                    Info &prv = pr[v];
                    prv.r += trans;
                    if (prv.r / g.degree(v) >= epsilon) big.insert(v);
                }
                ++it;
            }

            swap(pr, *ret);
            
        }


        public:

        Nibble (const Graph &g_, float alpha_, float epsilon_, unsigned maxit_)
            : g(g_), alpha(alpha_), epsilon(epsilon_), maxit(maxit_) {
        }

        void nibble (std::vector<ImageID> seed,
                        std::vector<ImageID> *result) {
            std::vector<ImageID> v;
            result->clear();

            BOOST_FOREACH(ImageID i, seed) {
                if (g.degree(i)) {
                    v.push_back(i);
                }
                else {
                    result->push_back(i);
                }
            }
            if (v.empty()) return;

            PageRank pr;
            rank(v, &pr);
            // check S^p_j
            // compute p = apr(a, Xv, r)
            std::vector<std::pair<float, ImageID> > sl;
            BOOST_FOREACH(const PageRank::value_type &vt, pr) {
                sl.push_back(std::make_pair(-vt.second.p / g.degree(vt.first), vt.first));
            }
            std::sort(sl.begin(), sl.end());

            //cerr << "!!" << sl.size() << "!!" << flush;

            unsigned top = 0, bottom = 0;

            float min_phi = std::numeric_limits<float>::max();
            unsigned min_idx = -1;

            std::set<ImageID> seen;

            for (unsigned i = 0; i < unsigned(sl.size()); ++i ) {
                unsigned cur = sl[i].second;
                bottom += g.degree(cur);
                BOOST_FOREACH(ImageID j, g.get(cur)) {
                    if (seen.count(j)) {
                        top -= 1;
                    }
                    else {
                        top += 1;
                    }
                }
                seen.insert(cur);

                float p = float(top)/float(bottom);
                if (p < min_phi) {
                    min_phi = p;
                    min_idx = i;
                }
            }

            //cerr << "**" << min_phi << "**" << flush << endl;
            
            for (unsigned i = 0; i <= min_idx; ++i) {
                result->push_back(sl[i].second);
            }
        }
    };
}

#endif

