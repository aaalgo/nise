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

#include "extract.h"
#include "../common/nise.h"

using namespace std;
namespace po = boost::program_options; 

static inline nise::Region MakeGeom (const Feature &f) {
    nise::Region m;
    m.x = f.x * f.scale;
    m.y = f.y * f.scale;
    m.r = f.size * f.scale;
    m.t = f.dir;
    return m;
}


typedef lshkit::Sketch<lshkit::DeltaLSB<lshkit::GaussianLsh> > Sketch;
int main(int argc, char **argv) {

    bool input_list = false;
    bool txt_sift = false;

    int max_size;
    int thumb;
    unsigned min_size;
    int O, S, o;
    float E, P, M, e;
    float log_base;
    float W;
    unsigned C;
    bool do_angle = true;
    Sketch sketch;

    int sample_method;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("min", po::value(&min_size)->default_value(nise::MIN_IMAGE_SIZE), "")
    ("max", po::value(&max_size)->default_value(nise::MAX_IMAGE_SIZE), "scale large images to this width/height")
    (",O", po::value(&O)->default_value(-1), "sift # octave")
    (",S", po::value(&S)->default_value(3), "sift S")
    (",o", po::value(&o)->default_value(0), "sift first octave")
    (",E", po::value(&E)->default_value(-1), "sift edge threshold")
    (",P", po::value(&P)->default_value(3), "sift peak threashold")
    (",M", po::value(&M)->default_value(-1), "sift magnif")
    (",l", po::value(&log_base)->default_value(nise::LOG_BASE), "")
    ("no-angle", "sift do angle")
    (",e", po::value(&e)->default_value(nise::MIN_ENTROPY), "sift entropy threshold")
    (",C", po::value(&C)->default_value(nise::MAX_FEATURES), "if non-zero, sample this number of features")
    ("lsh,W", po::value(&W)->default_value(nise::LSH_W), "")
    ("thumb", po::value(&thumb)->default_value(nise::THUMBNAIL_SIZE), "if non-zero, sample this number of features")
    ("sample", po::value(&sample_method)->default_value(SAMPLE_SIZE), "0: random, 1: by size")
    ("list", "read a list of paths from stdin")
    ("txt-sift", "")
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
    if (vm.count("no-angle")) {
        do_angle = false;
    }

    if (vm.count("list")) {
        input_list = true;
    }

    if (vm.count("txt-sift")) {
        txt_sift = true;
    }

    {
        lshkit::DefaultRng rng;
        Sketch::Parameter pm;
        pm.W = W;
        pm.dim = Sift::dim();
        sketch.reset(nise::SKETCH_SIZE, pm, rng);
    }

    Sift xtor(O, S, o, E, P, M, e, do_angle);

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
        //nise::Features results;
        //nise::Binary thumbnail;
        try {
            do {
                if (binary.empty()) break;
                Image im(&binary[0], binary.size(), max_size);

                vector<Feature> sift;
                if (im.width() < min_size) break;
                if (im.height() < min_size) break;
                xtor.extract(im, &sift);

                if (txt_sift) {
                    BOOST_FOREACH(const Feature &f, sift) {
                        cout << f.x << " " << f.y << " " << f.size << " " << f.scale;
                        BOOST_FOREACH(float v, f.desc) {
                            cout << " " << v;
                        }
                        cout << endl;
                    }
                    break;
                }

                record.meta.width = im.orig_width();
                record.meta.height = im.orig_height();
                record.meta.size = binary.size();
                nise::Checksum(binary, &record.checksum);

                if (log_base > 0) {
                    logscale(&sift, log_base);
                }
                if (C > 0) {
                    SampleFeature(&sift, C, sample_method);
                }
                record.regions.resize(sift.size());
                record.features.resize(sift.size());
                for (unsigned i = 0; i < sift.size(); ++i) {
                    record.regions[i] = MakeGeom(sift[i]);
                    sketch.apply(&sift[i].desc[0], record.features[i].sketch);
                }

                if (thumb > 0) {
                    Image tt(&binary[0], binary.size(), thumb, true);
                    tt.encode(&record.thumbnail);
                }
            } while (0);
        }
        catch (...) {
        }
        if (txt_sift) {
        }
        else {
            nise::Signature::RECORD.write(cout);
            record.write(cout);
        }
    }
    return 0;
}

