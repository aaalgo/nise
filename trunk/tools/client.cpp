#include <sstream>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <Poco/Runnable.h>
#include <Poco/Timespan.h>
#include <Poco/Thread.h>
#include <Poco/ThreadPool.h>
#include <Poco/Notification.h>
#include <Poco/NotificationQueue.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/StringPartSource.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/MediaType.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Path.h>
#include <Poco/URI.h>
#include <Poco/AutoPtr.h>
#include <Poco/Thread.h>
#include <Poco/Runnable.h>
#include <Poco/StreamCopier.h>
#include <Poco/NullStream.h>
#include <Poco/Exception.h>
#include "../common/nise.h"

namespace po = boost::program_options; 

static const long TIMEOUT = 100;

class Job: public Poco::Notification {
public:
    enum Type {
        URL = 0,
        IMAGE = 1,
        RECORD = 2,
        META = 3,
        THUMB = 4
    };

    Job (const nise::Record &rec): type(RECORD) {
        std::ostringstream ss(std::ios::binary);
        nise::Signature::RECORD.write(ss);
        rec.write(ss);
        string.assign(ss.str());
    }

    Job (Type typ, const std::string &str): type(typ), string(str) {
    }

    Job (Type typ, nise::ImageID id_): type(typ), id(id_) {
    }

    Type getType () const { return type; }
    const std::string &getString () const { return string; }
    nise::ImageID getID () const { return id; }
    void setTag (const std::string &t) {
        if (t.size()) {
            tag = t;
        }
    }
    void setTag (int t) {
        tag = boost::lexical_cast<std::string>(t);
    }
    const std::string &getTag () const {
        return tag;
    }
private:
    Type type;
    std::string string;
    nise::ImageID id;
    std::string tag;
};

class Output: public Poco::Notification {
    std::string string;
public:

    Output (const std::string &str): string(str) {
    }

    const std::string &get () const {
        return string;
    }
};

class InputReader: public Poco::Runnable {
public:
    enum Type {
        URL_LIST = 0,
        IMAGE_LIST = 1,
        RECORD_LIST = 2,
        RECORDS = 3,
        ID_LIST = 4
    };

    InputReader (Type t, std::istream &is, Poco::NotificationQueue &q)
        : type(t), input(is), queue(q) {
    }

    void run () {
        if (type == URL_LIST) {
            std::string url;
            while (input >> url) {
                Job* job = new Job(Job::URL, url);
                job->setTag(url);
                queue.enqueueNotification(job);
            }
        }
        else if (type == IMAGE_LIST) {
            std::string path;
            std::string image;
            while (input >> path) {
                nise::ReadFile(path, &image);
                if (image.empty()) {
                    std::cerr << "bad file" << std::endl;
                    continue;
                }
                Job* job = new Job(Job::IMAGE, image);
                job->setTag(path);
                queue.enqueueNotification(job);
            }
        }
        else if (type == RECORD_LIST) {
            std::string path;
            nise::Record record;
            while (input >> path) {
                std::ifstream is(path.c_str(), std::ios::binary);
                nise::Signature::RECORD.check(is);
                record.readFields(is);
                if (is) {
                    Job* job = new Job(record);
                    job->setTag(path);
                    queue.enqueueNotification(job);
                }
            }
        }
        else if (type == RECORDS) {
            int id = 0;
            for (;;) {
                nise::Record record;
                nise::Signature::RECORD.check(input);
                if (!input) break;
                record.readFields(input);
                if (!input) break;
                Job* job = new Job(record);
                job->setTag(id);
                queue.enqueueNotification(job);
                ++id;
            }
        }
        else if (type == ID_LIST) {
            nise::ImageID id;
            while (input >> id) {
                Job* job = new Job(Job::META, id);
                queue.enqueueNotification(job);
            }
        }
    }
private:
    Type type;
    std::istream &input;
    Poco::NotificationQueue &queue;
};

class OutputWriter: public Poco::Runnable {
    std::ostream &output;
    Poco::NotificationQueue &queue;
    bool *flag;
public:
    OutputWriter (std::ostream &os, Poco::NotificationQueue &q, bool *f)
        : output(os), queue(q), flag(f) {
    }

    void run () {
        while (!*flag) {
            Poco::AutoPtr<Output> out = dynamic_cast<Output *>(queue.waitDequeueNotification(TIMEOUT));
            if (out.isNull()) {
                if (*flag) break;
                continue;
            }
            output << out->get() << std::endl;
        }
    }
};

class Worker: public Poco::Runnable {
    std::string uri;
    std::string server;
    Poco::NotificationQueue &inQueue;
    Poco::NotificationQueue &outQueue;
    bool meta;
    bool thumb;
    bool *flag;

    void makeMoreQueries (const std::string &json, const std::string &tag) {
        size_t off = json.find("\"page\":[");
        if (off == json.npos) return;
        std::stringstream ss(json.substr(off + 8));
        int id;
        char c;
        for (;;) {
            ss >> id;
            if (!ss) {
                ss.clear();
                ss >> c;
                if (c == ']') break;
            }
            else {
                if (meta) {
                    Job* job = new Job(Job::META, id);
                    job->setTag(tag);
                    inQueue.enqueueUrgentNotification(job);
                }
                if (thumb) {
                    Job* job = new Job(Job::THUMB, id);
                    job->setTag(tag);
                    inQueue.enqueueUrgentNotification(job);
                }
            }
        }
    }
public:
    Worker (const std::string &serv, Poco::NotificationQueue &in, Poco::NotificationQueue &out, bool me, bool th, bool *f)
        : uri(serv), inQueue(in), outQueue(out), meta(me), thumb(th), flag(f) {
    }

    void run () {

        Poco::URI server(uri);

        for (;;) {
            Poco::AutoPtr<Job> job = dynamic_cast<Job *>(inQueue.waitDequeueNotification(TIMEOUT));
            if (job.isNull()) {
                if (*flag) break;
                continue;
            }
            Poco::Net::HTTPClientSession session(server.getHost(), server.getPort());

            Poco::Net::HTTPRequest req(Poco::Net::HTTPMessage::HTTP_1_1);
            Poco::Net::HTMLForm form;

            const std::string &tag = job->getTag();
            if (job->getType() == Job::URL) {
                req.setMethod(Poco::Net::HTTPRequest::HTTP_GET);
                req.setURI("/search/url");
                form.add("url", job->getString());
                form.add("batch", "1");
                form.add("tag", tag);
                form.add("log", "0");
            }
            else if (job->getType() == Job::IMAGE) {
                req.setMethod(Poco::Net::HTTPRequest::HTTP_POST);
                req.setURI("/search/upload");
                form.setEncoding(Poco::Net::HTMLForm::ENCODING_MULTIPART);
                form.add("batch", "1");
                form.add("tag", tag);
                form.add("log", "0");
                form.addPart("form-data",
                        new Poco::Net::StringPartSource(
                            job->getString(),
                            "image/jpeg",
                            "filename"));
            }
            else if (job->getType() == Job::RECORD) {
                req.setMethod(Poco::Net::HTTPRequest::HTTP_POST);
                req.setURI("/search/record");
                form.setEncoding(Poco::Net::HTMLForm::ENCODING_MULTIPART);
                form.add("batch", "1");
                form.add("tag", tag);
                form.add("log", "0");
                form.addPart("form-data",
                        new Poco::Net::StringPartSource(
                            job->getString(),
                            "application/octet-stream",
                            "filename"));
            }
            else if (job->getType() == Job::META) {
                req.setMethod(Poco::Net::HTTPRequest::HTTP_GET);
                req.setURI("/meta");
                form.add("id", boost::lexical_cast<std::string>(job->getID()));
                form.add("tag", tag);
            }
            else if (job->getType() == Job::THUMB) {
                req.setMethod(Poco::Net::HTTPRequest::HTTP_GET);
                req.setURI("/thumb");
                form.add("id", boost::lexical_cast<std::string>(job->getID()));
            }

            form.prepareSubmit(req);
            form.write(session.sendRequest(req));

            Poco::Net::HTTPResponse res;
            std::istream& rs = session.receiveResponse(res);
            std::string txt;
            Poco::StreamCopier::copyToString(rs, txt);
            if (job->getType() != Job::THUMB) {
                Poco::Notification::Ptr out = new Output(txt);
                outQueue.enqueueNotification(out);
            }
            if ((job->getType() == Job::IMAGE) || (job->getType() == Job::RECORD)) {
                makeMoreQueries(txt, job->getTag());
            }
        }
    }
};

int main(int argc, char **argv) {
    std::string url;
    int input_type;
    int num_threads;
    int meta;
    int thumb;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("url", po::value(&url)->default_value("http://simigle.com:8080"), "")
    ("type", po::value(&input_type)->default_value(3), "0: url list, 1: image path list, 2: record path list, 3: records, 4: ID list")
    ("thread", po::value(&num_threads)->default_value(1), "")
    ("meta", po::value(&meta)->default_value(0), "")
    ("thumb", po::value(&thumb)->default_value(0), "")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
        std::cerr << desc;
        return 1;
    }

    Poco::NotificationQueue inQueue;
    Poco::NotificationQueue outQueue;

    bool input_done = false;
    bool worker_done = false;

    std::vector<Poco::Thread*> threads;
    std::vector<Worker *> workers;

    for (int i = 0; i < num_threads; ++i)
    {
        Poco::Thread* pt = new Poco::Thread;
        poco_check_ptr(pt);
        threads.push_back(pt);
        Worker* worker = new Worker(url, inQueue, outQueue, meta, thumb, &input_done);
        poco_check_ptr(worker);
        workers.push_back(worker);
        pt->start(*worker);
    }

    Poco::Thread outputWriterThread;
    OutputWriter outputWriter(std::cout, outQueue, &worker_done);
    outputWriterThread.start(outputWriter);

    InputReader inputReader(InputReader::Type(input_type), std::cin, inQueue);
    inputReader.run();

    input_done = true;

    inQueue.wakeUpAll();

    BOOST_FOREACH(Poco::Thread *pt, threads) {
        pt->join();
        delete pt;
    }

    BOOST_FOREACH(Worker *worker, workers) {
        delete worker;
    }

    worker_done = true;

    outQueue.wakeUpAll();

    outputWriterThread.join();

    return 0;
}

