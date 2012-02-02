#ifndef WDONG_NISE
#define WDONG_NISE

/* This file defines data structures
 * and protocols used for communication
 * between various components of the search
 * engine.
 */
#include <cstdint>
#include <vector>
#include <iostream>
#include <map>
#include <boost/assert.hpp>
#include <boost/foreach.hpp>
#include <Poco/Bugcheck.h>

namespace nise {

    static const unsigned KILO = 1 << 10;
    static const unsigned MEGA = 1 << 20;
    static const unsigned GIGA = 1 << 30;

    // Default system parameters.

    static const unsigned THUMBNAIL_SIZE = 100;

    static const unsigned MAX_FEATURES = 200;
    static const unsigned MAX_BINARY = 100 * MEGA;
    static const unsigned MAX_SOURCES = 10000;
    
    static const unsigned SKETCH_BIT = 128;
    static const unsigned SKETCH_SIZE = 128/8;
    static const unsigned SKETCH_PLAN_DIST = 3;
    static const unsigned SKETCH_DIST = 4; // accept dist < 4
    static const unsigned SKETCH_TIGHT_DIST = 3; // accept dist < 4
    static const unsigned SKETCH_DIST_OFFLINE = 3;
    static const unsigned UPLOAD_BUFFER_SIZE = 8192;
    static const unsigned MAX_UPLOAD_SIZE = 1 * MEGA;

    static const unsigned MAX_HASH = 20000;

    static const unsigned DEMO_LIST_SIZE = 10;

    // feature extraction

    static const unsigned MIN_IMAGE_SIZE = 50;
    static const unsigned MIN_QUERY_IMAGE_SIZE = 20;
    static const unsigned QUERY_IMAGE_SCALE_THRESHOLD = 80;
    static const unsigned MAX_IMAGE_SIZE = 250;
    static const float MIN_ENTROPY = 4.4F;
    static const float LSH_W = 8.0F;
    static const float LOG_BASE = 0.001F;

    static const unsigned MAX_INITIAL_RESULTS = 2000;
    static const unsigned MAX_INITIAL_RESULTS_FOR_EXPAND = 1000;
    static const float NIBBLE_ALPHA = 0.5F;
    static const float NIBBLE_EPSILON = 0.00001F;
    static const unsigned NIBBLE_MAXIT = 1000;

    static const unsigned RECORD_CACHE_DEFAULT = 1024;
    static const unsigned RETRIEVAL_CACHE_DEFAULT = 1024;
    static const unsigned SESSION_EXPIRE_DEFAULT = 60000;
    static const unsigned TIME_LIMIT_DEFAULT = 1000;

    static const unsigned FBI_SKIP = 8;

    typedef uint32_t ImageID;

    static const unsigned IMAGE_ID_BIT = sizeof(ImageID) * 8;
    static const unsigned IMAGE_ID_OFFSET_BIT = 8;
    static const unsigned CONTAINER_SIZE = 1 << IMAGE_ID_OFFSET_BIT;
    static const unsigned IMAGE_ID_CONTAINER_BIT = IMAGE_ID_BIT - IMAGE_ID_OFFSET_BIT;

    class Environment {
        std::string home_;
        std::string hadoop_home_;
    public:
        Environment ();
        
        const std::string & home() const {
            return home_;
        }

        std::string jar_path () const;

        std::string hadoopHome () const {
            return hadoop_home_;
        }

        static bool insideHadoop () {
            return (getenv("hadoop.pipes.command.port") != NULL)
                || (getenv("mapreduce.pipes.command.port") != NULL);

        }
    };

    static inline ImageID MakeImageID (uint32_t container, uint32_t image) {
        BOOST_VERIFY(image < CONTAINER_SIZE);
        return (container << IMAGE_ID_OFFSET_BIT) | image;
    }

    static inline uint32_t ContainerID (ImageID id) {
        return id >> IMAGE_ID_OFFSET_BIT;
    }

    static inline uint32_t ContainerOffset (ImageID id) {
        return id & (CONTAINER_SIZE - 1);
    }
}

#include "io.h"

namespace nise {

    // A single feature.
   
    /* geometric information not supported for now
     
    struct Box {
        float left, right, top, bottom;
    };
    */
    struct Box {
        float left, right, top, bottom;
    };

    typedef uint8_t Chunk;
    typedef Chunk Sketch[SKETCH_SIZE];

    struct Region {
        float x, y, r, t;
    };

    struct Feature {
        Sketch sketch;
    };

    // A signature contains four bytes, used to
    // verify format correctness.
    class Signature {
    public:
        Signature (const char *text) // must contain 4 bytes
            : data(*reinterpret_cast<const uint32_t*>(text)) {
        }

        void check (std::istream &is) const {
            uint32_t d = ReadUint32(is);
            if (!is) return;
            if (d != data) is.setstate(std::ios::badbit);
        }

        bool check (uint32_t value) const {
            return value == data;
        }

        void write (std::ostream &os) const {
            WriteUint32(os, data);
        }

        static Signature IMAGE, FEATURES, RECORD, MAPPING, CONTAINER;
    private:
        uint32_t data;
    };

    // Feature extraction protocol
    // The feature extraction program expect a sequence of ImageBinary
    // input, each preceded by an ImageSignature.  For each input image,
    // it outputs a Features, precedied by FeaturesSignature.

    // Source of an indexed image.
    // An indexed record.
    struct Record {
        struct Meta {
            unsigned width;
            unsigned height;
            unsigned size;
        };
        struct Source {
            std::string url;
            std::string parentUrl;
        };

        Meta meta;
        std::string checksum;
        std::string thumbnail;
        std::vector<Region> regions;
        std::vector<Feature> features;
        std::vector<Source> sources;

        void clear () {
            meta.width = meta.height = meta.size = 0;
            checksum.clear();
            thumbnail.clear();
            regions.clear();
            features.clear();
            sources.clear();
        }

        void swap (Record &r) {
            Meta tmp = meta;
            meta = r.meta;
            r.meta = tmp;
            checksum.swap(r.checksum);
            thumbnail.swap(r.thumbnail);
            regions.swap(r.regions);
            features.swap(r.features);
            sources.swap(r.sources);
        }

        void readFields (std::istream &is) {
            is.read((char *)&meta, sizeof(Meta));
            ReadString(is, &checksum);
            ReadString(is, &thumbnail);
            ReadVector<Region>(is, &regions);
            ReadVector<Feature>(is, &features);
            uint32_t sz = ReadUint32(is);
            BOOST_VERIFY(sz <= MAX_SOURCES);
            sources.resize(sz);
            BOOST_FOREACH(Source &src, sources) {
                ReadString(is, &src.url);
                ReadString(is, &src.parentUrl);
            }
        }

        void write (std::ostream &os) const {
            os.write((const char *)&meta, sizeof(Meta));
            WriteString(os, checksum);
            WriteString(os, thumbnail);
            WriteVector<Region>(os, regions);
            WriteVector<Feature>(os, features);
            WriteUint32(os, sources.size());
            BOOST_FOREACH(const Source &src, sources) {
                WriteString(os, src.url);
                WriteString(os, src.parentUrl);
            }
        }
    };

    void Download (const std::string &url, std::string *file);

    void Checksum (const std::string &data, std::string *checksum);

    class Extension2Mime {
        std::string null;
        std::map<std::string, std::string> map;
        const std::string &_lookup (const std::string &v) const {
            std::map<std::string, std::string>::const_iterator
                it = map.find(v);
            if (it == map.end()) return null;
            return it->second;
        }
        static Extension2Mime instance;
    public:
        Extension2Mime(): null("test/plain") {
            map["html"] = "text/html";
            map["js"] = "text/javascript";
            map["css"] = "text/css";
            map["gif"] = "image/gif";
            map["jpg"] = "image/jpeg";
            map["json"] = "application/json";
        }
        static const std::string &lookup (const std::string &v) {
            size_t loc = v.rfind('.');
            return instance._lookup(v.substr(loc + 1));
        }
    };

    typedef std::pair<Feature, ImageID> HashEntry;
    // check the consistency of constants
    //
    class ConstantRangeCheck {
        static ConstantRangeCheck run;
    public:
        ConstantRangeCheck () {
            BOOST_VERIFY(MAX_FEATURES * sizeof(Feature) <= MAX_BINARY);
            BOOST_VERIFY(SKETCH_BIT / SKETCH_DIST_OFFLINE + 1 <= sizeof(uint64_t) * 8);
            BOOST_VERIFY(SKETCH_BIT / SKETCH_DIST_OFFLINE >= 8);
            BOOST_VERIFY(sizeof(HashEntry) == SKETCH_SIZE + sizeof(ImageID));
            BOOST_VERIFY(sizeof(Chunk) == 1);
        }
    };
}

#endif
