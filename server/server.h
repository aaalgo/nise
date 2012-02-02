#ifndef WDONG_NISE_SERVER
#define WDONG_NISE_SERVER

#include <Poco/UUID.h>
#include <Poco/Mutex.h>
#include <Poco/Runnable.h>
#include <Poco/ScopedLock.h>
#include <Poco/LRUCache.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/Stopwatch.h>
#include <Poco/Logger.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/FileChannel.h>
#include <Poco/AutoPtr.h>
#include <fbi.h>

#include "../image/extractor.h"
#include "expand.h"
#include "json.h"


namespace nise {

    class Timer {
        Poco::Stopwatch watch;
        int64_t t;
    public:
        Timer (unsigned milisecond): t(int64_t(milisecond) * 1000) {
            watch.start();
        }

        bool timeout () const {
            return (watch.elapsed() > t);
        }

        float elapsed () const {
            return watch.elapsed() / 1e6F;
        }
    };

    class Log {
        bool record;
        std::string dir;
        Poco::Logger *sys;
        Poco::Logger *acc;

        Log (): sys(0), acc(0)
        {
        }

        static Log inst;

        std::string checksumToPath (const std::string &cs) {
            std::string hex(StringToHex(cs));
            return dir + "/record/" + hex.substr(0, 2) + '/' + hex;
        }

        void initRecordDirectory () {
            static const char digits[] = "0123456789abcdef";
            if ((!record) || dir.empty()) return;
            {
                Poco::File sub(dir + "/record");
                if (sub.exists()) return;
                sub.createDirectory();
            }
            char path[11] = "/record/00";
            for (unsigned i = 0; i < 16; ++i) {
                path[8] = digits[i];
                for (unsigned j = 0; j < 16; ++j) {
                    path[9] = digits[j];
                    Poco::File sub(dir + path);
                    sub.createDirectory();
                }
            }
        }
    public:
        static void init (const Poco::Util::AbstractConfiguration &config) {
            BOOST_VERIFY(inst.sys == 0); 
            BOOST_VERIFY(inst.acc == 0); 
            inst.sys = &Poco::Logger::get("system");
            inst.acc = &Poco::Logger::get("access");
            inst.record = config.getBool("record", true);
            if (config.hasOption("nise.log.dir")) {
                inst.dir = config.getString("nise.log.dir");
                const std::string &fmt = config.getString("nise.log.format",
                        "[%q] %Y-%m-%d %H:%M:%S %t");
                {
                    Poco::AutoPtr<Poco::FileChannel> channel(new Poco::FileChannel);
                    channel->setProperty("path", inst.dir + "/system.log");
                    channel->setProperty("archive", "timestamp");
                    channel->setProperty("rotation", "monthly");
                    inst.sys->setChannel(new Poco::FormattingChannel(
                                new Poco::PatternFormatter(fmt),
                                channel
                    ));
                }

                {
                    Poco::AutoPtr<Poco::FileChannel> channel(new Poco::FileChannel);
                    channel->setProperty("path", inst.dir + "/access.log");
                    channel->setProperty("archive", "timestamp");
                    channel->setProperty("rotation", "daily");
                    inst.acc->setChannel(new Poco::FormattingChannel(
                                new Poco::PatternFormatter(fmt),
                                channel
                    ));
                }
                inst.initRecordDirectory();
            }
            system().information("Logging subsystem started.");
        }

        static void cleanup () {
            system().information("Logging subsystem stopped.");
            inst.sys = 0;
            inst.acc = 0;
        }

        static Poco::Logger &system () {
            return *inst.sys;
        }

        static Poco::Logger &access () {
            return *inst.acc;
        }

        static bool loadRecord (const std::string &checksum, Record *rec) {
            if ((!inst.record) || inst.dir.empty()) return false;
            std::ifstream is(inst.checksumToPath(checksum).c_str(), std::ios::binary);
            if (!is) return false;
            nise::Signature::RECORD.check(is);
            if (!is) return false;
            rec->readFields(is);
            return bool(is);
        }

        static void saveRecord (const Record &rec) {
            if ((!inst.record) || inst.dir.empty()) return;
            Poco::File file(inst.checksumToPath(rec.checksum));
            if (!file.exists()) {
                std::ofstream os(file.path().c_str(), std::ios::binary);
                nise::Signature::RECORD.write(os);
                rec.write(os);
            }
        }
    };

    // sketch database
    class SketchDB {
        fbi::DB db;
        Poco::Mutex mutex;

        SketchDB (const std::string &path)
            : db(path) {
        }

        ~SketchDB () {
        }

        static SketchDB *inst;
    public:
        static void init (const Poco::Util::AbstractConfiguration &config) {
            BOOST_VERIFY(inst == NULL);
            inst = new SketchDB(config.getString("nise.sketch.db"));
            Log::system().information("Sketch database started.");
        }

        static void cleanup (void) {
            BOOST_VERIFY(inst != 0);
            delete inst;
            inst = 0;
            Log::system().information("Sketch database stopped.");
        }

        static SketchDB &instance () {
            return *inst;
        }

        void search (const Feature &query, std::vector<ImageID> *result) {
            Poco::ScopedLock<Poco::Mutex> lock(mutex);
            fbi::Plan plan;
            db.plan(query.sketch, fbi::DB::SMART, SKETCH_PLAN_DIST, FBI_SKIP, &plan);
            db.run(query.sketch, SKETCH_DIST, plan, result);
        }

        void search (const std::vector<Feature> &query, std::vector<std::vector<ImageID> > *result) {
            Poco::ScopedLock<Poco::Mutex> lock(mutex);
            std::vector<fbi::Chunk*> queries(query.size());
            for (unsigned i = 0; i < query.size(); ++i) {
                queries[i] = const_cast<fbi::Chunk *>((const fbi::Chunk *)&(query[i].sketch[0]));
            }
            db.batch(queries, fbi::DB::SMART, SKETCH_PLAN_DIST, SKETCH_DIST, FBI_SKIP, result);
        }

        void stat (JSON &json) {
            json.beginObject("sketch.db");
            json.beginArray("stat");
            const std::vector<size_t> &stat = db.getStat();
            BOOST_FOREACH(size_t c, stat) {
                json.add(c);
            }
            json.endArray();
            json.endObject();
        }
    };

    // query expansion
    class Expansion {
        Graph graph;

        Expansion (const std::string &path): graph(path) {
        }

        ~Expansion () {
        }

        static Expansion *inst;
    public:
        static void init (const Poco::Util::AbstractConfiguration &config)
        {
            BOOST_VERIFY(inst == NULL);
            inst = new Expansion(config.getString("nise.expansion.db", ""));
            Log::system().information("Expansion database started.");
        }

        static void cleanup () {
            BOOST_VERIFY(inst != 0);
            delete inst;
            inst = 0;
            Log::system().information("Expansion database stopped.");
        }

        static Expansion &instance () {
            return *inst;
        }

        Graph::Range neighbors (ImageID v) const {
            return graph.get(v);
        }

        unsigned size () const {
            return graph.size();
        }

        void apply (std::vector<ImageID> *result) const {
            if (result->size() > MAX_INITIAL_RESULTS_FOR_EXPAND) {
                return;
            }
            if (result->empty()) {
                return;
            }
            std::vector<ImageID> v;
            swap(v, *result);
            Nibble nibble(graph, NIBBLE_ALPHA, NIBBLE_EPSILON, NIBBLE_MAXIT);
            nibble.nibble(v, result);
        }

    };

    // static content
    class StaticContent {
    public:
        struct Page {
            std::string mime;
            std::string content;
        };
    private:
        std::string root;
        bool preload;

        typedef std::map<std::string, Poco::SharedPtr<Page> > PageMap;
        PageMap map;

        StaticContent (const Poco::Util::AbstractConfiguration &config)
            : root(config.getString("nise.static.root")),
              preload(config.getBool("nise.static.preload")) 
        {
            if (!preload) return;
            Poco::File dir(root);
            if (dir.isDirectory()) {
                Poco::DirectoryIterator it(dir), end;
                while (it != end) {
                    Poco::File file(*it);
                    if (file.isFile()) {
                        Poco::SharedPtr<Page> page = map["/" + it.name()]
                                             = new Page;
                        page->mime = Extension2Mime::lookup(it.name());
                        ReadFile(file.path(), &page->content);
                    }
                    ++it;
                }
            }
        }

        ~StaticContent () {
        }

        static StaticContent *inst;
   public:

        static void init (const Poco::Util::AbstractConfiguration &config) {
            BOOST_VERIFY(inst == 0);
            inst = new StaticContent(config);
            Log::system().information("Static page server started.");
        }

        static void cleanup (void) {
            BOOST_VERIFY(inst != 0);
            delete inst;
            inst = 0;
            Log::system().information("Static page server stopped.");
        }

        static StaticContent &instance () {
            return *inst;
        }

        Poco::SharedPtr<Page> get (const std::string &url) {
            if (preload) {
                PageMap::const_iterator it = map.find(url);
                if (it == map.end()) return 0;
                return it->second;
            }
            else {
                std::string content;
                ReadFile(root + url, &content);
                if (!content.size()) return 0;
                else {
                    Page *page = new Page;
                    page->mime = Extension2Mime::lookup(url);
                    page->content.swap(content);
                    return page;
                }
            }
        }
    };

    class Demo: public std::vector<std::string> {
        std::string root;

        Demo (const Poco::Util::AbstractConfiguration &config)
            : root(config.getString("nise.demo.root"))
        {
            Poco::File dir(root);
            if (dir.isDirectory()) {
                Poco::DirectoryIterator it(dir), end;
                while (it != end) {
                    Poco::File file(*it);
                    if (file.isFile()) {
                        push_back(std::string());
                        ReadFile(file.path(), &back());
                    }
                    ++it;
                }
            }
            BOOST_VERIFY(size() > DEMO_LIST_SIZE);
        }

        static Demo *inst;
   public:

        static void init (const Poco::Util::AbstractConfiguration &config) {
            BOOST_VERIFY(inst == 0);
            inst = new Demo(config);
            Log::system().information("Demo server started.");
        }

        static void cleanup (void) {
            BOOST_VERIFY(inst != 0);
            delete inst;
            inst = 0;
            Log::system().information("Demo server stopped.");
        }

        static Demo &instance () {
            return *inst;
        }
    };

    class ImageDB{
        std::vector<uint64_t> index;
        std::ifstream input;

        Poco::LRUCache<ImageID, Record> cache;

        uint32_t total;

        Poco::Mutex mutex;

        void load (ImageID id) {
            uint32_t g_id = ContainerID(id);
            if (g_id >= index.size()) return;
            input.seekg(index[g_id]);
            Signature::CONTAINER.check(input);
            BOOST_VERIFY(input);
            uint32_t c_id = ReadUint32(input);
            /*uint32_t size =*/ ReadUint32(input);
            uint32_t cnt = ReadUint32(input);
            BOOST_VERIFY(g_id == c_id);
            for (unsigned i = 0; i < cnt; ++i) {
                ImageID id = ReadUint32(input);
                Record *record = new Record;
                record->readFields(input);
                cache.add(id, record);
            }
        }

        ImageDB (const Poco::Util::AbstractConfiguration &config)
            : cache(config.getInt("nise.image.cache", RECORD_CACHE_DEFAULT))
        {
            std::string index_path = config.getString("nise.image.index");
            std::string file = config.getString("nise.image.db");
            std::ifstream is(index_path.c_str(), std::ios::binary);
            BOOST_VERIFY(is);
            // determine the file size
            is.seekg(0, std::ios::end);
            std::streamoff size = is.tellg();
            BOOST_VERIFY(size % sizeof(index[0]) == 0);
            index.resize(size_t(size / sizeof(index[0])));
            // load index
            is.seekg(0, std::ios::beg);
            is.read((char *)&index[0], size);
            BOOST_VERIFY(is);
            // input data file
            input.open(file.c_str(), std::ios::binary);
            total = (index.size() - 1) * CONTAINER_SIZE;
        }

        ~ImageDB () {
        }

        static ImageDB *inst;
    public:

        static void init (const Poco::Util::AbstractConfiguration &config) {
            BOOST_VERIFY(inst == 0);
            inst = new ImageDB(config);
            Log::system().information("Image database started.");
        }

        static void cleanup (void) {
            BOOST_VERIFY(inst != 0);
            delete inst;
            inst = 0;
            Log::system().information("Image database stopped.");
        }

        static ImageDB &instance () {
            return *inst;
        }

        Poco::SharedPtr<Record> get (ImageID id, bool *hit) {
            Poco::ScopedLock<Poco::Mutex> lock(mutex);
            Poco::SharedPtr<Record> ptr = cache.get(id);
            if (!ptr.isNull()) {
                *hit = true;
                return ptr;
            }
            load(id);
            *hit = false;
            return cache.get(id);
        }

        uint32_t size() const {
            return total;
        }
    };

    class Retrieval {
        Record record;
        std::vector<std::vector<ImageID> > results;
        unsigned next;
        Poco::Mutex mutex;

        Retrieval () {
        }
    public:

        static Retrieval *fromRecord (Record &record) {
            Retrieval *r = new Retrieval;
            r->record.swap(record);
            r->results.resize(r->record.features.size());
            r->next = 0;
            return r;
        }

        ~Retrieval () {
        }

        unsigned size () const {
            return record.features.size();
        }

        bool done () const {
            return next >= record.features.size();
        }

        unsigned finished () const {
            return next;
        }

        const std::vector<ImageID> &last () const {
            BOOST_VERIFY(results.size() > 0);
            return results.back();
        }

        const std::vector<ImageID> &get (unsigned idx) const {
            BOOST_VERIFY(idx < results.size());
            return results[idx];
        }

        Poco::Mutex &getMutex () {
            return mutex;
        }

        void batch () {
            if (done()) return;
            SketchDB::instance().search(record.features, &results);
            next = record.features.size();
        }

        void progress () {
            //Poco::ScopedLock<Poco::Mutex> lock(mutex);
            if (done()) return;
            SketchDB::instance().search(record.features[next], &results[next]);
            ++next;
        }

        const Record &getRecord() const {
            return record;
        }
    };

    class RetrievalCache: public Poco::LRUCache<std::string, Retrieval> {

        static Poco::LRUCache<std::string, Retrieval> *inst;
    public:
        static void init (const Poco::Util::AbstractConfiguration &config) {
            BOOST_VERIFY(inst == 0);
            inst = new Poco::LRUCache<std::string, Retrieval>(config.getInt("nise.retrieval.cache",
                                                                RETRIEVAL_CACHE_DEFAULT));
            Log::system().information("Retrieval cache started.");
        }

        static void cleanup (void) {
            BOOST_VERIFY(inst != 0);
            delete inst;
            inst = 0;
            Log::system().information("Retrieval cache stopped.");
        }

        static Poco::LRUCache<std::string, Retrieval> &instance () {
            return *inst;
        }

    };

    class Session {
    public:
        enum Type {
            RANDOM = 0,
            LOCAL = 1,
            IMAGE = 2
        };

        struct Parameter {
            bool expansion;
            bool batch;
            Box box;
            bool crop;
        };

    private:

        Poco::UUID id;

        Type method;

        ImageID local;
        Poco::SharedPtr<Retrieval> retrieval;

        Parameter param;

        bool done;
        unsigned empty;
        unsigned finished;

        std::set<ImageID> seen;
        std::vector<ImageID> results;

        float time;
        // A session can be accessed and manipulated by one thread
        Poco::Mutex mutex;

        static Poco::UUIDGenerator uuid;

        void sync () {
            poco_check_ptr(retrieval);
            const Record &record = retrieval->getRecord();
            poco_assert(record.regions.size() == record.features.size());
            while (finished < retrieval->finished()) {
                if (retrieval->get(finished).empty()) {
                    ++empty;
                }
                else {
                    const Region &region = record.regions[finished];

                    bool in = (region.x >= param.box.left * record.meta.width) 
                        && (region.x <= param.box.right * record.meta.width)
                        && (region.y >= param.box.top * record.meta.height)
                        && (region.y <= param.box.bottom * record.meta.height);

                    if (param.crop == in) {
                        BOOST_FOREACH(ImageID v, retrieval->get(finished)) {
                            if (!seen.count(v)) {
                                seen.insert(v);
                                results.push_back(v);
                            }
                        }
                    }
                }
                ++finished;
            }
        }
    public:
        Session (const Parameter &p)
            : id(uuid.create()), method(RANDOM), local(0), param(p), done(false), empty(0), finished(0) {
        }

        Session (ImageID id, const Parameter &p)
            : id(uuid.create()), method(LOCAL), local(id), param(p), done(false), empty(0), finished(0)
        {
        }

        Session (const std::string &query, const Parameter &p)
            : id(uuid.create()), method(IMAGE), local(0), param(p), done(false), empty(0), finished(0)
        {
            std::string checksum;
            Checksum(query, &checksum);

            retrieval = RetrievalCache::instance().get(checksum);

            if (retrieval.isNull()) {
                Extractor xtor;
                Record record;
                xtor.extract(query, &record, true);
                retrieval = Retrieval::fromRecord(record);
                RetrievalCache::instance().add(checksum, retrieval);
            }
        }

        Session (Record &query, const Parameter &p)
            : id(uuid.create()), method(IMAGE), local(0), param(p), done(false), empty(0), finished(0)
        {
            retrieval = RetrievalCache::instance().get(query.checksum);

            if (retrieval.isNull()) {
                retrieval = Retrieval::fromRecord(query);
                RetrievalCache::instance().add(query.checksum, retrieval);
            }
        }

        ~Session () {
        }

        Type getType () const {
            return method;
        }

        const Poco::UUID &getID () const {
            return id;
        }

        void update (Parameter &new_param) {
            Poco::ScopedLock<Poco::Mutex> lock(mutex);
            param = new_param;
            done = false;
            finished = 0;
            seen.clear();
            results.clear();
        }

        const Poco::SharedPtr<Retrieval> &getRetrieval () const {
            return retrieval;
        }

        void run (unsigned goal, Timer &timer) {
            Poco::ScopedLock<Poco::Mutex> lock(mutex);
            if (!done) {
                if (method == RANDOM) {
                    results.resize(goal);
                    ImageID max = ImageDB::instance().size();
                    Expansion &exp = Expansion::instance();

                    if (exp.size() * 10 < max) {
                        BOOST_FOREACH(ImageID &v, results) {
                            v = rand() % max;
                        }
                    }
                    else {
                        BOOST_FOREACH(ImageID &v, results) {
                            for (;;) {
                                v = rand() % max;
                                Graph::Range range = exp.neighbors(v);
                                if (range.second - range.first > 5) break;
                            }
                        }
                    }
                }
                else if (method == LOCAL) {
                    if (param.expansion) {
                        results.push_back(local);
                        Expansion::instance().apply(&results);
                    }
                    else {
                        Graph::Range range = Expansion::instance().neighbors(local);
                        results.push_back(local);
                        BOOST_FOREACH(ImageID id, range) {
                            results.push_back(id);
                        }
                    }
                    done = true;
                }
                else if (method == IMAGE) {
                    Poco::ScopedLock<Poco::Mutex> lock(retrieval->getMutex());
                    sync();
                    for (;;) {
                        if (retrieval->done()) break;
                        if ((results.size() > goal) && timer.timeout()) break;
                        if (param.batch) {
                            retrieval->batch();
                        }
                        else {
                            retrieval->progress();
                        }
                        sync();
                    }
                    if (retrieval->done()) {
                        if (param.expansion) {
                            Expansion::instance().apply(&results);
                        }
                        done = true;
                    }
                }
                else BOOST_VERIFY(0);
                time = timer.elapsed();
            }
        }

        void serve (std::ostream &os, unsigned start, unsigned count, const std::string &tag = "") {
            Poco::ScopedLock<Poco::Mutex> lock(mutex);

            unsigned max_count = 0;
            if (start < results.size()) {
                max_count = results.size() - start;
            }
            if (count > max_count) count = max_count;
            if (count == 0) count = max_count;

            JSON json(os);
            json.add("done", done);


            if (!retrieval.isNull()) {
                const Record &record = retrieval->getRecord();
                json.add("thumb", true)
                    .add("image.width", record.meta.width)
                    .add("image.height", record.meta.height)
                    .add("retrieve.total", retrieval->size())
                    .add("retrieve.done", finished)
                    .add("retrieve.empty", empty)
                    .add("sha1", StringToHex(record.checksum));
            }
            else {
                json.add("thumb", false);
            }

            if (method == LOCAL) {
                json.add("tiny", "/thumb?id=" + boost::lexical_cast<std::string>(local));
            } 
            else if (method != RANDOM) {
                json.add("tiny", "/search/thumb?id=" + id.toString());
            }

            if (!tag.empty()) {
                json.add("tag", tag);
            }

            json.add("id", id.toString())
                .add("time", time)
                .add("num_results", results.size())
                .add("page_offset", start)
                .add("page_count", count)
                .beginArray("page");
            for (unsigned i = start; i < start + count; ++i) {
                json.add(results[i]);
            }
            json.endArray();
        }
    };

    class SessionCache {
        static Poco::AccessExpireCache<Poco::UUID, Session> *inst;
    public:
        static void init (const Poco::Util::AbstractConfiguration &config) {
            BOOST_VERIFY(inst == 0);
            inst = new Poco::AccessExpireCache<Poco::UUID, Session>(config.getInt("nise.session.expire",
                                                                SESSION_EXPIRE_DEFAULT));
            Log::system().information("Session cache started.");
        }

        static void cleanup (void) {
            BOOST_VERIFY(inst != 0);
            delete inst;
            inst = 0;
            Log::system().information("Session cache stopped.");
        }

        static Poco::AccessExpireCache<Poco::UUID, Session> &instance () {
            
            return *inst;
        }
    };

    class BackgroundFetcher: public Poco::Runnable {
        
        bool done;
    public:
        BackgroundFetcher (): done(false) {
        }

        void stop () {
            done = true;
        }

        void run () {
            Log::system().information("Background fetcher started.");
			while (!done) {
#ifdef WIN32
				Sleep(1000);
#else
                sleep(1);
#endif
            }
            Log::system().information("Background fetcher stopped.");
        }
    };
}


#endif

