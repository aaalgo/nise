// This program tries to find matches between local
// interesting points from two images using
// various methods.
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <string>
#include <boost/foreach.hpp>
#include <boost/array.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <lshkit.h>
#include <mkl_cblas.h>
#include "image.h"
#include "pca.h"


using namespace imgddup;
namespace po = boost::program_options; 

static const unsigned DIM = 128;
static const unsigned MATCH_TRUE = 0;
static const unsigned MATCH_SQ = 1;
static const unsigned MATCH_VQ = 2;
static const unsigned MATCH_PCA = 3;
static const unsigned MATCH_SKETCH = 4;
static const unsigned MATCH_LSH = 5;
static const unsigned MATCH_SH = 6;
static const unsigned MATCH_LSH_SKETCH = 7;

struct Parameter {
    // match true
    unsigned stage;
    unsigned K;
    std::vector<float> T;

    //sketch
    float sketch_W;
    unsigned sketch_M;

    //LSH
    unsigned lsh_H;
    unsigned lsh_L;
    float lsh_W;
    unsigned lsh_M;
    

    // raw
    std::string pca_path;
    unsigned pca_dim;
    float pca_max;
    float pca_scale;
    unsigned pca_quant;

    // match sq
    unsigned short Q;
    unsigned max_l1;

    // match vq
    std::string dict;
    unsigned soft;

    // feature extraction
    unsigned min_size;
    int max_size;   // maximal width/height
    int O, S, o;
    float E, P, M, e;
    int do_angle;
    std::string black_list;
    float black_threshold;

    float log_base;
    unsigned C;
    unsigned sample_method;
    float coverage;
    unsigned match_method;
    unsigned N;
    std::string fg_dir;
    std::string bg_dir;
    std::string fg_path;
    std::string bg_path;
    std::string out_path;

    void addOptions (boost::program_options::options_description *desc) {
        desc->add_options()
        ("min", po::value(&min_size)->default_value(50), "")
        ("max", po::value(&max_size)->default_value(250), "scale large images to this width/height")
        (",O", po::value(&O)->default_value(-1), "sift # octave")
        (",S", po::value(&S)->default_value(3), "sift S")
        (",o", po::value(&o)->default_value(0), "sift first octave")
        (",E", po::value(&E)->default_value(-1), "sift edge threshold")
        (",P", po::value(&P)->default_value(3), "sift peak threashold")
        (",M", po::value(&M)->default_value(-1), "sift magnif")
        (",l", po::value(&log_base)->default_value(0.001), "")
        ("angle", po::value(&do_angle)->default_value(1), "sift do angle (0/1)")
        (",B", po::value(&black_list), "black list")
        ("bt", po::value(&black_threshold)->default_value(200), "")
        (",e", po::value(&e)->default_value(4.4), "sift entropy threshold")
        ("maxl1", po::value(&max_l1)->default_value(12), "max l1 distance")
        (",Q", po::value(&Q)->default_value(1), "quantizer")
        (",K", po::value(&K)->default_value(1000), "true ransac loop")
        (",T", po::value(&T), "true threshold")
        ("stage", po::value(&stage)->default_value(2), "")
        (",C", po::value(&C)->default_value(0), "if non-zero, sample this number of features")
        ("dict", po::value(&dict)->default_value("/memex/wdong/src/bang/c++/expr/result/means.19"), "")
        ("soft", po::value(&soft)->default_value(4), "")
        ("match", po::value(&match_method)->default_value(MATCH_SQ), "0: true 1: SQ")
        ("sample", po::value(&sample_method)->default_value(SAMPLE_SIZE), "0: random, 1: by size, 2: tree")
        ("coverage", po::value(&coverage)->default_value(1.2))
        (",N", po::value(&N)->default_value(10), "# fg to use as query")
        ("fgdir", po::value(&fg_dir), "")
        ("bgdir", po::value(&bg_dir), "")
        ("fg", po::value(&fg_path), "")
        ("bg", po::value(&bg_path), "")
        ("pca-path", po::value(&pca_path)->default_value("/memex/wdong/src/bang/data/sift.pca"), "")
        ("pca-dim", po::value(&pca_dim)->default_value(0), "")
        ("pca-max", po::value(&pca_max)->default_value(20.0f), "")
        ("pca-scale", po::value(&pca_scale)->default_value(1.0f), "")
        ("pca-quant", po::value(&pca_quant)->default_value(1), "")
        ("sketch-W", po::value(&sketch_W)->default_value(1.0f), "")
        ("sketch-M", po::value(&sketch_M)->default_value(16), "")
        ("lsh-W", po::value(&lsh_W)->default_value(1.0f), "")
        ("lsh-M", po::value(&lsh_M)->default_value(1.0f), "")
        ("lsh-L", po::value(&lsh_L)->default_value(4), "")
//        ("lsh-H", po::value(&lsh_H)->default_value(750000007), "")
//        104,395,301
        ("lsh-H", po::value(&lsh_H)->default_value(4550163), "")
        ("output", po::value(&out_path), "");
    }
};

struct Transform {
    float s, x, y;

    Transform () : s(1.0), x(0), y(0) {
    }
    Transform (const Feature &p1, const Feature &p2) {
        s = p2.size / p1.size;
        x = p2.x - p1.x * s;
        y = p2.y - p1.y * s;
    }
    float error (const Feature &p1, const Feature &p2) {
        float ss = p1.size * s;
        float xx = p1.x * s + x;
        float yy = p1.y * s + y;
        return sqrt(lshkit::sqr(ss - p2.size)
                + lshkit::sqr(xx - p2.x)
                + lshkit::sqr(yy - p2.y));
    }
};

class MatchTrue {
    unsigned stage;
    unsigned K;
    std::vector<float> T;
    typedef std::pair<unsigned, unsigned> MatchID;
public:

    typedef std::vector<Feature> Meta;

    MatchTrue (const Parameter &param) : stage(param.stage), K(param.K), T(param.T) {
        if (T.empty()) T.push_back(1.0);
        /*
        T.resize(param.T.size());
        for (unsigned i = 0; i < T.size(); ++i) {
            T[i] = boost::lexical_cast<float>(param.T[i]);
        }
        */
    }

    unsigned size () const {
        if (stage == 1) return 1;
        else return T.size();
    }

    void preproc (const std::vector<Feature> &feature, Meta *meta) const {
        *meta = feature;
    }

    unsigned match (const Meta &l1, const Meta &l2, unsigned *ret) const {
        std::vector<MatchID> matches;
        lshkit::metric::l2sqr<float> l2sqr(DIM);

        // This is D. Lower's method of finding matches
        // can be over-strict, and we'll only use this
        // for RANSAC.
        for (unsigned i = 0; i < l1.size(); ++i) {
            unsigned match = 0;
            float mind, second;
            mind = second = std::numeric_limits<float>::max();
            for (unsigned j = 0; j < l2.size(); ++j) {
    //            if (l1[i].laplacian != l2[j].laplacian) continue;

                float d = l2sqr(&l1[i].desc[0], &l2[j].desc[0]);
                if (d < mind) {
                    second = mind;
                    mind = d;
                    match = j;
                }
                else if (d < second) {
                    second = d;
                }
            }
            if (mind < 0.25 * second) {
                matches.push_back(std::make_pair(i, match));
            }
        }

        if (stage == 1) {
            *ret = matches.size();
            return 0;
        }
        // RANSAC
        unsigned b_C = 0;
        Transform b_tr;

        std::fill(ret, ret + T.size(), 0);

        if (matches.size() == 0) {
            return 0;
        }

        for (unsigned k = 0; k < K; ++k) {
            unsigned l = rand() % matches.size();
            Transform tr(l1[matches[l].first], l2[matches[l].second]);
            unsigned C = 0;
            BOOST_FOREACH(MatchID m, matches) {
                float delta = tr.error(l1[m.first], l2[m.second]);
                if (delta <= T[0]) {
                    ++C;
                }
            }
            if (C > b_C) {
                b_C = C;
                b_tr = tr;
            }
        }

        if (stage == 2) {
            BOOST_FOREACH(MatchID m, matches) {
                float delta = b_tr.error(l1[m.first], l2[m.second]);
                for (unsigned i = 0; i < T.size(); ++i) {
                    if (delta <= T[i]) {
                        ret[i]++;
                        break;
                    }
                }
            }
        }
        else {
       
        // Now we have a transform, we'll use that to find
        // the true matches.  We define true matches to be
        // interesting points with matching location,
        // not considering the local feature.
            for (unsigned i = 0; i < l1.size(); ++i) {
                for (unsigned j = 0; j < l2.size(); ++j) {
                    float delta = b_tr.error(l1[i], l2[j]);
                    for (unsigned k = 0; k < T.size(); ++k) {
                        if (delta <= T[k]) {
                            ret[k]++;
                            break;
                        }
                    }
                }
            }
        }
        return 0;
    }
};

typedef boost::array<unsigned short, DIM> SIFT_SQ;

class MatchPCA {
    PCA *pca;

    float max, scale;
    unsigned umax;
    bool quant;
    lshkit::metric::l2<float> l2d;
public:

    typedef std::vector<Feature> Meta;

    MatchPCA (const Parameter &param): pca(0), max(param.pca_max), scale(param.pca_scale), umax(std::ceil(param.pca_max * param.pca_scale)), quant(param.pca_quant), l2d(param.pca_dim ? param.pca_dim: Sift::dim()) {
        if (param.pca_dim > 0) {
            pca = new PCA(param.pca_path);
        }
    }

    unsigned size () const {
        return umax;
    }

    void preproc (const std::vector<Feature> &feature, Meta *meta) const {
        if (pca == 0) {
            *meta = feature;
            return;
        }
        meta->resize(feature.size());
        for (unsigned i = 0; i < feature.size(); ++i) {
            meta->at(i).desc.resize(Sift::dim());
            pca->apply(&feature[i].desc[0], &meta->at(i).desc[0], quant);
        }
    }

    unsigned match (const Meta &l1, const Meta &l2, unsigned *ret) const {
        std::fill(ret, ret + umax, 0);
        for (unsigned i = 0; i < l1.size(); ++i) {
            for (unsigned j = 0; j < l2.size(); ++j) {
                float d = l2d(&l1[i].desc[0], &l2[j].desc[0]);
                if (d < max) {
                    d *= scale;
                    ret[unsigned(floor(d))]++;
                }
            }
        }
        return 0;
    }
};

class MatchSQ {
    unsigned short Q;
    unsigned max_l1;
public:

    typedef std::vector<SIFT_SQ> Meta;

    MatchSQ (const Parameter &param) : Q(param.Q), max_l1(param.max_l1) {
        BOOST_VERIFY(Q != 0xFFFFU);
    }

    unsigned size () const {
        return max_l1;
    }

    void preproc (const std::vector<Feature> &feature, Meta *meta) const {
        meta->resize(feature.size());
        for (unsigned i = 0; i < feature.size(); ++i) {
            for (unsigned j = 0; j < DIM; ++j) {
                float q = round(feature[i].desc[j] * Q);
                BOOST_VERIFY(q <= Q);
                meta->at(i)[j] = q;
            }
        }
    }

    unsigned match (const Meta &l1, const Meta &l2, unsigned *ret) const {
        std::fill(ret, ret + max_l1, 0);
        for (unsigned i = 0; i < l1.size(); ++i) {
            for (unsigned j = 0; j < l2.size(); ++j) {
                unsigned d = 0;
                for (unsigned k = 0; k < DIM; ++k) {
                    if (l1[i][k] == l2[j][k]) continue;
                    else if (((l1[i][k] + 1) ==  l2[j][k]) || (l1[i][k] == (l2[j][k] + 1))) {
                        d++;
                        if (d >= max_l1) break;
                    }
                    else {
                        d = max_l1;
                        break;
                    }
                }
                if (d < max_l1) {
                    ++ret[d];
                }
            }
        }
        return 0;
    }
};

class MatchSketch {
    typedef lshkit::Sketch<lshkit::DeltaLSB<lshkit::GaussianLsh> > Sketch;

    unsigned M;
    unsigned max_l1;
    Sketch sketch;
public:

    typedef std::vector<std::vector<unsigned char> > Meta;

    MatchSketch (const Parameter &param): M(param.sketch_M), max_l1(param.max_l1) {
        lshkit::DefaultRng rng;
        Sketch::Parameter pm;
        pm.W = param.sketch_W;
        pm.dim = Sift::dim();
        sketch.reset(M, pm, rng);
    }

    unsigned size () const {
        return max_l1;
    }

    void preproc (const std::vector<Feature> &feature, Meta *meta) const {
        meta->resize(feature.size());
        for (unsigned i = 0; i < feature.size(); ++i) {
            meta->at(i).resize(M);
            sketch.apply(&feature[i].desc[0], &meta->at(i)[0]);
        }
    }

    unsigned match (const Meta &l1, const Meta &l2, unsigned *ret) const {
        lshkit::metric::hamming<unsigned char> hamming(M);
        std::fill(ret, ret + max_l1, 0);
        for (unsigned i = 0; i < l1.size(); ++i) {
            for (unsigned j = 0; j < l2.size(); ++j) {
                unsigned d = hamming(&l1[i][0], &l2[j][0]);
                if (d < max_l1) {
                    ret[d]++;
                }
            }
        }
        return 0;
    }
};

class MatchSH {

    unsigned M;
    unsigned max_l1;
    lshkit::SpectralHash sketch;
public:

    typedef std::vector<std::vector<unsigned char> > Meta;

    MatchSH (const Parameter &param):  max_l1(param.max_l1) {
        std::ifstream is("/memex/wdong/src/bang/c++/sh.data", std::ios::binary);
        sketch.serialize(is, 0);
        M = sketch.getChunks();
    }

    unsigned size () const {
        return max_l1;
    }

    void preproc (const std::vector<Feature> &feature, Meta *meta) const {
        meta->resize(feature.size());
        for (unsigned i = 0; i < feature.size(); ++i) {
            meta->at(i).resize(M);
            sketch.apply(&feature[i].desc[0], &meta->at(i)[0]);
        }
    }

    unsigned match (const Meta &l1, const Meta &l2, unsigned *ret) const {
        lshkit::metric::hamming<unsigned char> hamming(M);
        std::fill(ret, ret + max_l1, 0);
        for (unsigned i = 0; i < l1.size(); ++i) {
            for (unsigned j = 0; j < l2.size(); ++j) {
                unsigned d = hamming(&l1[i][0], &l2[j][0]);
                if (d < max_l1) {
                    ret[d]++;
                }
            }
        }
        return 0;
    }
};

static inline float n2sqr (const float *p1) {
    float l = 0;
    unsigned d;
    // we hard coded DIM so this loop can be unrolled better by the compiler
    for (d = 0; d < DIM; d++) {
        l += p1[d] * p1[d];
    }
    return l;
}

class MatchVQ {
    unsigned soft;
public:

    typedef std::vector<unsigned> Meta;

    static const unsigned SOFT = 4;
    struct Pre {
        float x, y, size, scale;
        unsigned words[SOFT];
    };


private:
    unsigned K;
    std::vector<float> means;
    std::vector<float> means_n2sqr;
public:
    MatchVQ (const Parameter &param) : soft(param.soft)  {
        std::ifstream is(param.dict.c_str(), std::ios::binary);
        BOOST_VERIFY(is);
        is.seekg(0, std::ios::end);
        K = is.tellg() / sizeof(float) / DIM;
        means.resize(DIM * K);
        is.seekg(0, std::ios::beg);
        is.read((char *)&means[0], means.size() * sizeof(float));
        BOOST_VERIFY(is);

        means_n2sqr.resize(K);
        for (unsigned i = 0; i < K; ++i) {
            means_n2sqr[i] = n2sqr(&means[i * DIM]);

        }
        BOOST_VERIFY(soft <= K);
    }

    void preproc (const std::vector<Feature> &feature, Meta *meta) const {
        std::vector<float> ft;
        std::vector<float> dot;

        meta->resize(feature.size() * soft);

        ft.resize(DIM * feature.size());
        for (unsigned i = 0; i < feature.size(); ++i) {
            copy(feature[i].desc.begin(), feature[i].desc.end(), ft.begin() + i * DIM);
        }

        //memset(&dot[0], 0, K * feature.size() * sizeof(float));
        dot.resize(K * feature.size());
        for (unsigned i = 0; i < feature.size(); ++i) {
            copy(means_n2sqr.begin(), means_n2sqr.end(), dot.begin() + i * K);
        }
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, feature.size(), K, DIM, -2.0, (const float *)&ft[0], DIM, &means[0], DIM, 1.0, &dot[0], K);

        for (unsigned i = 0; i < feature.size(); ++i) {
            float *l2 = &dot[0] + i * K;
            unsigned *idx = &meta->at(0) + i * soft;

            for (unsigned j = 0; j < soft; ++j) {
                unsigned k = j; 
                for (;;) {
                    if (k == 0) break;
                    if (l2[idx[k-1]] >= l2[j]) break;
                    idx[k] = idx[k-1];
                    --k;
                }
                idx[k] = j;
            }

            for (unsigned j = soft; j < K; ++j) {
                if (l2[j] >= l2[idx[0]]) continue;
                unsigned k = 0;
                for (;;) {
                    if (k + 1 >= soft) break;
                    if (l2[idx[k+1]] <= l2[j]) break;
                    idx[k] = idx[k+1];
                    ++k;
                }
                idx[k] = j;
            }
        }
    }


    unsigned size () const {
        return 1;
    }


    unsigned match (const Meta &l1, const Meta &l2, unsigned *ret) const {
        unsigned r = 0;
        for (unsigned i = 0; i < l1.size(); ++i) {
            for (unsigned j = 0; j < l2.size(); ++j) {
                if (l1[i] == l2[j]) ++r;
            }
        }
        *ret = r;
        return 0;
    }
};


struct Hashing {
    unsigned h;
    unsigned v;
};

Hashing MakeHashing (unsigned h, unsigned v) {
    Hashing ha;
    ha.h = h;
    ha.v = v;
    return ha;
}

bool operator < (const Hashing &h1, const Hashing &h2) {
    //return h1.h < h2.h;
    if (h1.h < h2.h) return true;
    if (h1.h > h2.h) return false;
    return h1.v < h2.v;
}

class MatchLSH {
    typedef lshkit::MultiProbeLsh<> LSH;
    std::vector<LSH> lshs;
    float max, scale;
    unsigned umax;
    lshkit::metric::l2<float> l2d;

public:
    struct Meta {
        std::vector<std::vector<float> > features;
        std::vector<Hashing > hashes;
    };

    // lsh_L, sketch_M, sketch_W, pca_max, pca_scale

    MatchLSH (const Parameter &param): lshs(param.lsh_L), max(param.pca_max), scale(param.pca_scale), umax(std::ceil(param.pca_max * param.pca_scale)), l2d(Sift::dim()) {
        LSH::Parameter pr;

        pr.W = param.lsh_W;
        pr.range = param.lsh_H; // See H in the program parameters.  You can just use the default value.
        pr.repeat = param.lsh_M;
        pr.dim = Sift::dim();
        lshkit::DefaultRng rng;

        BOOST_FOREACH(LSH &lsh, lshs) {
            lsh.reset(pr, rng);
        }
    }

    unsigned size () const {
        return umax;
    }

    void preproc (const std::vector<Feature> &feature, Meta *meta) const {
        meta->hashes.clear();
        meta->hashes.resize(lshs.size());
        meta->features.clear();
        for (unsigned i = 0; i < feature.size(); ++i) {
            meta->features.push_back(feature[i].desc);
            for (unsigned j = 0; j < lshs.size(); ++j) {
                unsigned h = lshs[j](&feature[i].desc[0]) * lshs.size() + j;
                meta->hashes.push_back(MakeHashing(h, i));
            }
        }
        std::sort(meta->hashes.begin(), meta->hashes.end());
    }

    unsigned match (const Meta &l1, const Meta &l2, unsigned *ret) const {
        std::fill(ret, ret + umax, 0);
        std::vector<Hashing> matches;

        unsigned a = 0, b = 0;
        const std::vector<Hashing> &ha = l1.hashes, &hb = l2.hashes;
        for (;;) {
            if (a >= ha.size()) break;
            if (b >= hb.size()) break;
            if (ha[a].h < hb[b].h) {
                ++a;
                continue;
            }
            if (ha[a].h > hb[b].h) {
                ++b;
                continue;
            }
            unsigned ap = a + 1;
            unsigned bp = b + 1;
            while ((ap < ha.size()) && (ha[ap].h == ha[a].h)) ++ap;
            while ((bp < hb.size()) && (hb[bp].h == hb[b].h)) ++bp;
            for (unsigned ax = a; ax < ap; ++ax)
                for (unsigned bx = b; bx < bp; ++bx) 
                    matches.push_back(MakeHashing(ha[ax].v, hb[bx].v));
            a = ap;
            b = bp;
        }

        std::sort(matches.begin(), matches.end());
        for (unsigned i = 0; i < matches.size(); ++i) {
            if ((i + 1 >= matches.size())
                    || (matches[i].h != matches[i+1].h)
                    || (matches[i].v != matches[i+1].v)) {
                float d = l2d(&l1.features[matches[i].h][0], &l2.features[matches[i].v][0]);
                if (d < max) {
                    d *= scale;
                    ret[unsigned(floor(d))]++;
                }
            }
        }
        return matches.size();
    }
};

class MatchLSHSketch {
    typedef lshkit::MultiProbeLsh<> LSH;
    typedef lshkit::Sketch<lshkit::DeltaLSB<lshkit::GaussianLsh> > Sketch;
    std::vector<LSH> lshs;
    unsigned M;
    unsigned max_l1;
    Sketch sketch;

public:
    struct Meta {
        std::vector<std::vector<unsigned char> > features;
        std::vector<Hashing> hashes;
    };

    // lsh_L, sketch_M, sketch_W, pca_max, pca_scale

    MatchLSHSketch (const Parameter &param): lshs(param.lsh_L), M(param.sketch_M), max_l1(param.max_l1) {
        LSH::Parameter pr;

        pr.W = param.lsh_W;
        pr.range = param.lsh_H; // See H in the program parameters.  You can just use the default value.
        pr.repeat = param.lsh_M;
        pr.dim = Sift::dim();
        lshkit::DefaultRng rng;

        BOOST_FOREACH(LSH &lsh, lshs) {
            lsh.reset(pr, rng);
        }

        Sketch::Parameter pm;
        pm.W = param.sketch_W;
        pm.dim = Sift::dim();
        sketch.reset(M, pm, rng);
    }

    unsigned size () const {
        return max_l1;
    }

    void preproc (const std::vector<Feature> &feature, Meta *meta) const {
        meta->features.resize(feature.size());
        meta->hashes.clear();
        for (unsigned i = 0; i < feature.size(); ++i) {
            meta->features.at(i).resize(M);
            sketch.apply(&feature[i].desc[0], &meta->features.at(i)[0]);
            for (unsigned j = 0; j < lshs.size(); ++j) {
                unsigned h = lshs[j](&feature[i].desc[0]) * lshs.size() + j;
                meta->hashes.push_back(MakeHashing(h, i));
            }
        }
        std::sort(meta->hashes.begin(), meta->hashes.end());
    }

    unsigned match (const Meta &l1, const Meta &l2, unsigned *ret) const {
        lshkit::metric::hamming<unsigned char> hamming(M);
        std::fill(ret, ret + max_l1, 0);
        std::vector<Hashing> matches;

        unsigned a = 0, b = 0;
        const std::vector<Hashing> &ha = l1.hashes, &hb = l2.hashes;
        for (;;) {
            if (a >= ha.size()) break;
            if (b >= hb.size()) break;
            if (ha[a].h < hb[b].h) {
                ++a;
                continue;
            }
            if (ha[a].h > hb[b].h) {
                ++b;
                continue;
            }
            unsigned ap = a + 1;
            unsigned bp = b + 1;
            while ((ap < ha.size()) && (ha[ap].h == ha[a].h)) ++ap;
            while ((bp < hb.size()) && (hb[bp].h == hb[b].h)) ++bp;
            for (unsigned ax = a; ax < ap; ++ax)
                for (unsigned bx = b; bx < bp; ++bx) 
                    matches.push_back(MakeHashing(ha[ax].v, hb[bx].v));
            a = ap;
            b = bp;
        }

        std::sort(matches.begin(), matches.end());
        for (unsigned i = 0; i < matches.size(); ++i) {
            if ((i + 1 >= matches.size())
                    || (matches[i].h != matches[i+1].h)
                    || (matches[i].v != matches[i+1].v)) {
                unsigned d = hamming(&l1.features[matches[i].h][0], &l2.features[matches[i].v][0]);
                if (d < max_l1) {
                    ret[d]++;
                }
            }
        }
        return matches.size();
    }
};

// Simulation framework
//
class SimBase {
public:
    virtual void run () = 0;
};

template <typename MATCHER>
class Sim: public SimBase, protected Parameter, private MATCHER
{
    Sift sift;

    typedef typename MATCHER::Meta Meta;

    struct ImageMeta {
        std::string name;
        Meta meta;
    };

    std::ofstream feature_size;
    bool loadImage (const std::string &path, Meta *meta) {
        try {
            Image image(path, max_size);
            if ((image.width() < min_size)
                    || (image.height() < min_size)) {
                return false;
            }
            std::vector<Feature> ft;
            sift.extract(image, &ft);
            if (ft.empty()) return false;
            if (log_base > 0) {
                logscale(&ft, log_base);
            }
            if (C > 0) {
                if (sample_method == 2) {
                    TrimFeature(&ft, C, coverage);
                }
                else {
                    SampleFeature(&ft, C, sample_method);
                }
            }
            MATCHER::preproc(ft, meta);
            feature_size << ft.size() << std::endl;
            return true;
        }
        catch (...) {
            return false;
        }
    }

    // load a list of foreground images
    void loadFG (std::vector<ImageMeta> *list) {
        list->clear();
        std::ifstream is(fg_path.c_str());
        std::string id;
        std::string p;
        while (is >> id >> p) {
            size_t slash = p.find_first_of('/');
            if (slash != std::string::npos) {
                id = p.substr(0, slash + 1) + id;
            }
            Meta meta;
            if (loadImage(fg_dir + p, &meta)) {
                list->push_back(ImageMeta());
                list->back().name = id;
                list->back().meta = meta;
            }
        }
    }

public:

    Sim (const Parameter &sp)
        : Parameter(sp), MATCHER(sp), sift(sp.O, sp.S, sp.o, sp.E, sp.P, sp.M, sp.e, sp.do_angle){
        if (black_list.size()) {
            sift.setBlackList(black_list, black_threshold);
        }
    }

    virtual void run () {
        std::ofstream os(out_path.c_str());
        std::ofstream s_out((out_path + ".size").c_str());
        feature_size.open((out_path + ".fs").c_str());

        if (bg_path == "") {
            std::vector<ImageMeta> list;
            loadFG(&list);
            std::vector<unsigned> cnt(MATCHER::size());

            for (unsigned i = 0; i < N; ++i) {
                for (unsigned j = 0; j < list.size(); ++j) {
                    if (i == j) continue;
                    MATCHER::match(list[i].meta, list[j].meta, &cnt[0]);
                    std::accumulate(cnt.begin(), cnt.end(), 0);
                    if (true) { // always show negative for queries
                        os << list[i].name << '\t' << list[j].name;
                        for (unsigned k = 0; k < cnt.size(); ++k) {
                            os << '\t' << cnt[k];
                        }
                        os << std::endl;
                    }
                }
            }
        }
        else {
            std::vector<ImageMeta> list;
            loadFG(&list);
            std::vector<unsigned> cnt(MATCHER::size());

            std::ifstream is(bg_path.c_str());
            std::string p;
            std::string id;
            Meta bg;
            float cost = 0;

            while (is >> id >> p) {
                if (loadImage(bg_dir + p, &bg)) {
                    for (unsigned i = 0; i < list.size(); ++i) {
                        unsigned r = MATCHER::match(list[i].meta, bg, &cnt[0]);
                        cost += r;
                        unsigned total = std::accumulate(cnt.begin(), cnt.end(), 0);
                        if (total > 0) {
                            os << list[i].name << '\t' << id;
                            for (unsigned k = 0; k < cnt.size(); ++k) {
                                os << '\t' << cnt[k];
                            }
                            os << std::endl;
                        }
                    }
                    s_out << cost << std::endl;
                }
            }
        }
        os.close();
        s_out.close();
    }
};


using namespace std;
using namespace imgddup;

int main(int argc, char **argv) {

    Parameter sp;

    po::options_description desc("Allowed options");

    sp.addOptions(&desc);
    desc.add_options()
            ("help,h", "produce help message.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("fg") != 1) || vm.count("output") != 1) {
        cerr << desc;
        return 1;
    }

    SimBase *sim = 0;

    if (sp.match_method == MATCH_TRUE) {
        sim = new Sim<MatchTrue>(sp);
    }
    else if (sp.match_method == MATCH_SQ) {
        sim = new Sim<MatchSQ>(sp);
    }
    else if (sp.match_method == MATCH_VQ) {
        sim = new Sim<MatchVQ>(sp);
    }
    else if (sp.match_method == MATCH_PCA) {
        sim = new Sim<MatchPCA>(sp);
    }
    else if (sp.match_method == MATCH_SKETCH) {
        sim = new Sim<MatchSketch>(sp);
    }
    else if (sp.match_method == MATCH_LSH) {
        sim = new Sim<MatchLSH>(sp);
    }
    else if (sp.match_method == MATCH_SH) {
        sim = new Sim<MatchSH>(sp);
    }
    else if (sp.match_method == MATCH_LSH_SKETCH) {
        sim = new Sim<MatchLSHSketch>(sp);
    }
    BOOST_VERIFY(sim);

    sim->run();

    delete sim;
    return 0;
}

