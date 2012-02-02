#ifndef WDONG_NISE_PAGE
#define WDONG_NISE_PAGE

#include <Poco/Net/MediaType.h>
#include "Poco/Buffer.h"

namespace nise {

    POCO_DECLARE_EXCEPTION(, WebInputException, Poco::ApplicationException);
    POCO_DECLARE_EXCEPTION(, NotFoundException, Poco::ApplicationException);

    class WebInput
    {
    public:
        struct Upload {
            Poco::Net::MessageHeader header;
            std::string body;
        };
    private:

        // Upload list & handler
        class Uploads: public std::vector<Upload>,
                       public Poco::Net::PartHandler
        {
            unsigned num;
        public:
            Uploads (unsigned n): num(n) {
            }

            void handlePart(const Poco::Net::MessageHeader& header,
                            std::istream& istr) {
                if (size() >= num) {
                    throw WebInputException("too many uploads");
                }
                //std::cerr << "file received" << std::endl;
                push_back(Upload());
                Upload &cur = back();
                cur.header = header;

                std::string &str = cur.body;
                Poco::Buffer<char> buffer(UPLOAD_BUFFER_SIZE);
                std::streamsize len = 0;
                istr.read(buffer.begin(), UPLOAD_BUFFER_SIZE);
                std::streamsize n = istr.gcount();
                while (n > 0)
                {
                    len += n;
                    if (len > MAX_UPLOAD_SIZE) {
                        throw WebInputException("file too large");
                    }
                    str.append(buffer.begin(), static_cast<std::string::size_type>(n));
                    if (istr)
                    {
                        istr.read(buffer.begin(), UPLOAD_BUFFER_SIZE);
                        n = istr.gcount();
                    }
                    else n = 0;
                }
            }
        };

        Uploads uploads;
        Poco::Net::HTMLForm form;

    public:
        WebInput (Poco::Net::HTTPServerRequest & request,
                unsigned num_upload):
            uploads(num_upload),
            form(request, request.stream(), uploads)
        {
            if (uploads.size() != num_upload) {
                throw WebInputException("too few uploads.");
            }
        }

        const std::vector<Upload> &getUploads () const {
            return uploads;
        }

        template <typename T>
        const T get (const std::string &name) const {
            auto it = form.find(name);
            if (it == form.end()) {
                throw WebInputException("parameter " + name + "not found.");
            }
            const std::string &value = it->second;
            try {
                return boost::lexical_cast<T>(value);
            }
            catch (const boost::bad_lexical_cast &) {
                throw WebInputException("parameter " + name + "has bad value (" + value + ")");
            }
        }

        template <typename T>
        const T get (const std::string &name, const T &def) const {
            auto it = form.find(name);
            if (it == form.end()) {
                return def;
            }
            const std::string &value = it->second;
            try {
                return boost::lexical_cast<T>(value);
            }
            catch (const boost::bad_lexical_cast &) {
                throw WebInputException("ineter " + name + "has bad value.");
            }
        }

        const std::string &get (const std::string &name) const {
            auto it = form.find(name);
            if (it == form.end()) {
                throw WebInputException("ineter " + name + "not found.");
            }
            return it->second;
        }

        const std::string &get (const std::string &name, const std::string &def) const {
            auto it = form.find(name);
            if (it == form.end()) {
                return def;
            }
            return it->second;
        }
    };

    class Page {
    protected:
        std::string tag;

    public:
        enum ContentType {
            CONTENT_HTML = 0,
            CONTENT_JPEG,
            CONTENT_JSON
        };
    private:
        void sendErrorMessage (Poco::Net::HTTPServerResponse& response,
                               const std::string &message) const {
            response.setStatus(Poco::Net::HTTPServerResponse::HTTP_BAD_REQUEST);
            switch (contentType()) {
                case CONTENT_HTML:
                        response.send() << "<HTML><BODY>" << message
                                       << "</BODY></HTML>" << std::endl;
                    break;
                case CONTENT_JPEG:
                    break;
                case CONTENT_JSON:
                        JSON json(response.send());
                        json.add("error", message);
                    break;
            }
        }
    public:

        virtual Page *construct () const = 0;

        virtual bool log () const {
            return true;
        }
        
        virtual std::string logMsg () const {
            return "";
        }

        virtual ContentType contentType () const {
            return CONTENT_HTML;
        }

        virtual unsigned numUploads () const  {
            return 0;
        }

        virtual void input (const WebInput& in) {
            tag = in.get<std::string>("tag", "");
        }

        virtual void run () {
        }

        virtual void output (std::ostream &os) {
        }

        void serve (Poco::Net::HTTPServerRequest& request,
                    Poco::Net::HTTPServerResponse& response) {
            do {    // so we can use break to go to the end
                try {
                    WebInput in(request, numUploads());
                    input(in);
                } catch (const WebInputException &e) {
                    sendErrorMessage(response, e.displayText());
                    break;
                }
                try {
                    run();
                } catch (const std::exception &e) {
                    sendErrorMessage(response, e.what());
                    break;
                }
                try {
                    switch (contentType()) {
                        case CONTENT_HTML:  response.setContentType(
                                            Poco::Net::MediaType("text/html"));
                                    break;
                        case CONTENT_JSON:  //response.setContentType(
                                            //Poco::Net::MediaType("application/json"));
                                    break;
                        case CONTENT_JPEG:  
                                            response.setContentType(
                                            Poco::Net::MediaType("image/jpeg"));
                                    break;
                        default:
                                    throw Poco::LogicException("undefined content type");
                    }
                    output(response.send());
                }
                catch (const std::exception &e) {
                    Log::system().error(e.what());
                    break;
                }
            } while (0);
            if (log()) {
                Log::access().information(
                        boost::lexical_cast<std::string>(response.getStatus())
                        + ' ' + request.clientAddress().toString()
                        + ' ' + request.getMethod()
                        + ' ' + request.getURI()
                        + " '" + request.get("User-Agent", "")
                        + "; " + logMsg());
            }
        }
    };

    class DemoImagePage: public Page {
        unsigned id;
    public:
        virtual bool log () const {
            return false;
        }

        virtual Page *construct () const {
            return new DemoImagePage;
        }

        virtual ContentType contentType () const {
            return CONTENT_JPEG;
        }

        virtual void input (const WebInput &in) {
            id = in.get<unsigned>("id");
            if (id >= Demo::instance().size()) {
                throw WebInputException("invalid demo id");
            }
        }

        virtual void output (std::ostream &os) {
            const std::string &file = Demo::instance()[id];
            os.write(&file[0], file.size());
        }
    };

    class DemoListPage: public Page {
    public:
        virtual Page *construct () const {
            return new DemoListPage;
        }

        virtual ContentType contentType () const {
            return CONTENT_JSON;
        }

        virtual void output (std::ostream &os) {
            std::vector<unsigned> list(Demo::instance().size());
            for (unsigned i = 0; i < list.size(); ++i) {
                list[i] = i;
            }
            std::random_shuffle(list.begin(), list.end());
            list.resize(DEMO_LIST_SIZE);
            JSON json(os);
            json.beginArray("page");
            BOOST_FOREACH(unsigned v, list) {
                json.add(v);
            }
            json.endArray();
        }
    };

    class StatPage: public Page {
    public:
        virtual bool log () const {
            return false;
        }

        virtual Page *construct () const {
            return new StatPage;
        }

        virtual ContentType contentType () const {
            return CONTENT_JSON;
        }

        virtual void output (std::ostream &os) {
            nise::JSON json(os);
            SketchDB::instance().stat(json);
        }
    };

    class DataPage: public Page {
    protected:
        ImageID id;
        Poco::SharedPtr<Record> record;
        bool hit;
    public:

        virtual bool log () const {
            return false;
        }

        DataPage() : hit(false) {}

        virtual void input (const WebInput& in) {
            Page::input(in);
            id = in.get<ImageID>("id");
            hit = 0;
        }
        virtual void run () {
            record = ImageDB::instance().get(id, &hit);
            if (record.isNull()) {
                throw NotFoundException("record not found.");
            }
        }
    };

    class MetaPage: public DataPage {
    public:
        virtual Page *construct () const {
            return new MetaPage;
        }

        virtual ContentType contentType () const {
            return CONTENT_JSON;
        }
        virtual void output (std::ostream &os) {
            poco_check_ptr(record);
            nise::JSON json(os);
            json.add("hit", hit)
                .add("id", id)
                .add("width", record->meta.width)
                .add("height", record->meta.height)
                .add("size", record->meta.size);
            if (!tag.empty()) {
                json.add("tag", tag);
            }
            json.beginArray("link");
            BOOST_FOREACH(const Record::Source &src, record->sources) {
                json.add(src.url);
            }
            json.endArray();
        }
    };

    class ThumbPage: public DataPage {
    public:
        virtual Page *construct () const {
            return new ThumbPage;
        }

        virtual ContentType contentType () const {
            return CONTENT_JPEG;
        }
        virtual void output (std::ostream &os) {
            poco_check_ptr(record);
            os.write(&record->thumbnail[0], record->thumbnail.size());
        }
    };

    class ResultPage: public Page {
    protected:
        bool dolog;
        unsigned start;
        unsigned count;
        unsigned time_limit;
        Poco::SharedPtr<Session> session;
    public:

        virtual ContentType contentType () const {
            return CONTENT_JSON;
        }
        virtual void input (const WebInput& in) {
            Page::input(in);
            start = in.get<unsigned>("page_offset", 0);
            count = in.get<unsigned>("page_count", 0);
            time_limit = in.get<unsigned>("time_limit", TIME_LIMIT_DEFAULT);
            dolog = (in.get<int>("log", 1) != 0);
        }
        virtual void run () {
            Timer timer(time_limit);
            if (session->getType() == Session::RANDOM) start = 0;
            session->run(start + count, timer);
        }
        virtual void output (std::ostream &os) {
            session->serve(os, start, count, tag);
        }

        virtual bool log () const {
            return dolog;
        }

        virtual std::string logMsg () const {
            if (!session.isNull()) {
                Poco::SharedPtr<Retrieval> rt = session->getRetrieval();
                if (!rt.isNull()) {
                    return StringToHex(rt->getRecord().checksum);
                }
            }
            return "-";
        }
    };

    class SearchPage: public ResultPage {
    protected:
        Session::Parameter param;
    public:
        virtual void input (const WebInput& in) {
            ResultPage::input(in);
//            param.loose = in.get<unsigned>("threshold", SKETCH_DIST) - SKETCH_TIGHT_DIST;
//            if (param.loose > 1) {throw WebInputException("Threshold not supported.");}

            param.expansion = (0 != in.get<int>("expansion", 1));
            param.batch = (0 != in.get<int>("batch", 0));
            param.box.left = in.get<float>("box.left", 0.0);
            param.box.top = in.get<float>("box.top", 0.0);
            param.box.right = in.get<float>("box.right", 1.0);
            param.box.bottom = in.get<float>("box.bottom", 1.0);
            param.crop = (in.get<int>("crop", 1) != 0);
        }
    };

    class SearchURLPage: public SearchPage {
        std::string url;
    public:
        virtual Page *construct () const {
            return new SearchURLPage;
        }
        virtual void input (const WebInput& in) {
            SearchPage::input(in);
            url = in.get("url");
            std::string query;
            Download(url, &query);
            if (query.empty()) {
                throw WebInputException("bad URL");
            }
            session = new Session(query, param);
            poco_check_ptr(session);
            SessionCache::instance().add(session->getID(), session);
            if (log()) { Log::saveRecord(session->getRetrieval()->getRecord()); }
        }
    };

    class SearchSHA1Page: public SearchPage {
    public:
        virtual Page *construct () const {
            return new SearchSHA1Page;
        }
        virtual void input (const WebInput& in) {
            SearchPage::input(in);
            std::string checksum(HexToString(in.get("sha1")));
            Record record;
            if (Log::loadRecord(checksum, &record)) {
                session = new Session(record, param);
                poco_check_ptr(session);
                SessionCache::instance().add(session->getID(), session);
            }
            else {
                throw WebInputException("record not found.");
            }
        }
    };

    class SearchUploadPage: public SearchPage {
    public:
        virtual Page *construct () const {
            return new SearchUploadPage;
        }
        virtual unsigned numUploads () const  {
            return 1;
        }

        virtual void input (const WebInput& in) {
            SearchPage::input(in);
            const std::string &query = in.getUploads()[0].body;
            session = new Session(query, param);
            poco_check_ptr(session);
            SessionCache::instance().add(session->getID(), session);
            if (log()) { Log::saveRecord(session->getRetrieval()->getRecord()); }
        }
    };

    class SearchDemoPage: public SearchPage {
    public:
        virtual Page *construct () const {
            return new SearchDemoPage;
        }

        virtual void input (const WebInput& in) {
            SearchPage::input(in);
            unsigned id = in.get<unsigned>("id");
            if (id >= Demo::instance().size()) {
                throw WebInputException("invalid demo id");
            }
            session = new Session(Demo::instance()[id], param);
            poco_check_ptr(session);
            SessionCache::instance().add(session->getID(), session);
            if (log()) { Log::saveRecord(session->getRetrieval()->getRecord()); }
        }
    };

    class SearchRecordPage: public SearchPage {
    public:
        virtual Page *construct () const {
            return new SearchRecordPage;
        }
        virtual unsigned numUploads () const  {
            return 1;
        }
        virtual void input (const WebInput& in) {
            SearchPage::input(in);
            std::stringstream ss(in.getUploads()[0].body);
            Signature::RECORD.check(ss);
            Record record;
            record.readFields(ss);
            if (!ss) throw WebInputException("bad record");
            session = new Session(record, param);
            poco_check_ptr(session);
            SessionCache::instance().add(session->getID(), session);
            if (log()) { Log::saveRecord(session->getRetrieval()->getRecord()); }
        }
    };

    class SearchLocalPage: public SearchPage {
    protected:
        ImageID id;
    public:
        virtual Page *construct () const {
            return new SearchLocalPage;
        }
        virtual void input (const WebInput &in) {
            SearchPage::input(in);
            id = in.get<unsigned>("id");
            session = new Session(id, param);
            poco_check_ptr(session);
            SessionCache::instance().add(session->getID(), session);
        }
    };

    class SearchRandomPage: public SearchPage {
    public:
        virtual Page *construct () const {
            return new SearchRandomPage;
        }
        virtual void input (const WebInput &in) {
            SearchPage::input(in);
            start = 0;
            session = new Session(param);
            poco_check_ptr(session);
            SessionCache::instance().add(session->getID(), session);
        }
    };
   
    class SearchUpdatePage: public SearchPage {
    public:
        virtual Page *construct () const {
            return new SearchUpdatePage;
        }
        virtual void input (const WebInput &in) {
            SearchPage::input(in);
            start = 0;
            const std::string &id_txt = in.get("id");
            Poco::UUID id(id_txt);
            session = SessionCache::instance().get(id);
            if (session.isNull()) {
                throw WebInputException("session timeout");
            }
            session->update(param);
        }
    };

    class SearchFollowUpPage: public SearchPage {
    public:
        virtual Page *construct () const {
            return new SearchFollowUpPage;
        }
        virtual void input (const WebInput &in) {
            SearchPage::input(in);
            const std::string &id_txt = in.get("id");
            Poco::UUID id(id_txt);
            session = SessionCache::instance().get(id);
            if (session.isNull()) {
                throw WebInputException("session timeout");
            }
        }
    };

    class QueryThumbPage: public Page {
        Poco::SharedPtr<Retrieval> ret;
    public:
        virtual bool log () const {
            return false;
        }
        virtual Page *construct () const {
            return new QueryThumbPage;
        }
        virtual ContentType contentType () const {
            return CONTENT_JPEG;
        }
        virtual void input (const WebInput &in) {
            const std::string &id_txt = in.get("id");
            Poco::UUID id(id_txt);
            Poco::SharedPtr<Session> session = SessionCache::instance().get(id);
            if (session.isNull()) {
                throw WebInputException("session timeout");
            }
            ret = session->getRetrieval();
            if (ret.isNull()) {
                throw WebInputException("session without retrieval");
            }
            if (ret->getRecord().thumbnail.empty()) {
                throw WebInputException("bad thumbnail");
            }
        }
        virtual void output (std::ostream &os) {
            const std::string &thumbnail = ret->getRecord().thumbnail;
            os.write(&thumbnail[0], thumbnail.size());
        }
    };

    class DynamicContent {
        std::map<std::string, Poco::SharedPtr<Page> > map;
        static DynamicContent inst;
        DynamicContent () {
            map["/stat"] = new StatPage;
            map["/thumb"] = new ThumbPage;
            map["/meta"] = new MetaPage;
            map["/demo/list"] = new DemoListPage;
            map["/demo/image"] = new DemoImagePage;
            map["/search/sha1"] = new SearchSHA1Page;
            map["/search/url"] = new SearchURLPage;
            map["/search/upload"] = new SearchUploadPage;
            map["/search/record"] = new SearchRecordPage;
            map["/search/random"] = new SearchRandomPage;
            map["/search/demo"] = new SearchDemoPage;
            map["/search/local"] = new SearchLocalPage;
            map["/search/update"] = new SearchUpdatePage;
            map["/search/page"] = new SearchFollowUpPage;
            map["/search/thumb"] = new QueryThumbPage;
        }
    public:
        static void init (const Poco::Util::AbstractConfiguration &config) {
            Log::system().information("Dynamic page server started.");
        }
        static void cleanup () {
            Log::system().information("Dynamic page server stopped.");
        }
        static Page *construct (const std::string &path) {
            auto it = inst.map.find(path);
            if (it == inst.map.end()) return 0;
            return it->second->construct();
        }
    };
}

#endif

