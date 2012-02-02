#ifndef WDONG_NISE_EXTRACTOR
#define WDONG_NISE_EXTRACTOR

#include "../common/nise.h"

namespace nise {

    struct ExtractorImpl;

    class Extractor {
        ExtractorImpl *impl;
    public:
        Extractor ();
        void extract (const std::string &image, Record *record, bool query = true);
        ~Extractor ();
    };
}

#endif
