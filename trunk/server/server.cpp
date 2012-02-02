#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <Poco/AccessExpireCache.h>
#include <Poco/Exception.h>
#include <Poco/ScopedLock.h>
#include <Poco/SharedPtr.h>
#include <Poco/SharedMemory.h>
#include <Poco/StreamCopier.h>
#include <Poco/UUID.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/Thread.h>
#include <Poco/ThreadPool.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/MessageHeader.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPSessionFactory.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/ServerApplication.h>
#include "../common/nise.h"
#include "server.h"
#include "page.h"

namespace nise {

// query cache
Log Log::inst; 
SketchDB *SketchDB::inst;
Expansion *Expansion::inst;
StaticContent *StaticContent::inst;
ImageDB * ImageDB::inst;
DynamicContent DynamicContent::inst;
Demo * Demo::inst;
Poco::LRUCache<std::string, Retrieval> *RetrievalCache::inst;
Poco::AccessExpireCache<Poco::UUID, Session> *SessionCache::inst;
Poco::UUIDGenerator Session::uuid;

POCO_IMPLEMENT_EXCEPTION(WebInputException, Poco::ApplicationException, "WEB INPUT");
POCO_IMPLEMENT_EXCEPTION(NotFoundException, Poco::ApplicationException, "NOT FOUND");

class HTTPRequestHandler: public Poco::Net::HTTPRequestHandler {

public:
    void handleRequest (Poco::Net::HTTPServerRequest& request,
                        Poco::Net::HTTPServerResponse& response) {
        std::string uri = request.getURI();

        if (request.getHost().find('3') != std::string::npos) {
            response.setContentType(
            return;
        }

        {
            Poco::SharedPtr<Page> page = DynamicContent::construct(Poco::URI(uri).getPath());
            if (!page.isNull()) {
                page->serve(request, response);
                return;
            }
        }
        
        {
            if (uri == "/") {
                const std::string &lang = request.get("Accept-Language", "");
                if (lang.find("zh") != lang.npos) {
                    uri = "/index-cn.html";
                }
                else {
                    uri = "/index.html";
                }
            }
            const Poco::SharedPtr<StaticContent::Page> page(StaticContent::instance().get(uri));
            if (!page.isNull()) {
                response.setContentType(page->mime);
                response.send().write(&page->content[0], page->content.size());
                return;
            }
        }
        response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
    }
};

class RequestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory {
public:
    Poco::Net::HTTPRequestHandler*
        createRequestHandler(const Poco::Net::HTTPServerRequest& request)
    {
        return new HTTPRequestHandler;
    }
};


class Server: public Poco::Util::ServerApplication
{
public:
    Server(): _helpRequested(false)
    {
    }
    
protected:
    void initialize(Poco::Util::Application& self)
    {
        loadConfiguration(); // load default configuration files, if present
        ServerApplication::initialize(self);
        Log::init(self.config());

    }
        
    void uninitialize()
    {
        ServerApplication::uninitialize();
        Log::cleanup();
    }

    void defineOptions(Poco::Util::OptionSet& options)
    {
        Poco::Util::ServerApplication::defineOptions(options);
        
        options.addOption(
            Poco::Util::Option("help", "h", "display help information on command line arguments")
                .required(false)
                .repeatable(false)
                .callback(Poco::Util::OptionCallback<Server>(this, &Server::handleHelp)));
        options.addOption(
            Poco::Util::Option("graph", "g", "run as the graph server")
                .required(false)
                .repeatable(false)
                .binding("nise.server.graph", &config()));
    }

    void handleHelp(const std::string& name, const std::string& value)
    {
        _helpRequested = true;
        Poco::Util::HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("OPTIONS");
        helpFormatter.setHeader("A sample server application that demonstrates some of the features of the Util::ServerApplication class.");
        helpFormatter.format(std::cout);
        stopOptionsProcessing();
    }

    void dropPrivilege () {
#ifndef WIN32
        if (getuid() == 0) {
            const std::string &user = config().getString("nise.server.user", "nobody");
            struct passwd *ent = getpwnam(user.c_str());
            BOOST_VERIFY(ent);
            setuid(ent->pw_uid);
        }
#endif
    }

    int main(const std::vector<std::string>& args)
    {
        if (!_helpRequested)
        {
            if (config().hasOption("nise.server.graph")) {
            // run in graph server mode
                Poco::SharedMemory shared(config().getString("nise.expansion.db"), Poco::SharedMemory::AM_READ);
                Log::system().information("Running in graph server mode.");
                waitForTerminationRequest();
                return 0;
            }

            Log::system().information("Starting...");
            ImageDB::init(config());
            SketchDB::init(config());
            Expansion::init(config());
            RetrievalCache::init(config());
            SessionCache::init(config());
            StaticContent::init(config());
            DynamicContent::init(config());
            Demo::init(config());

            Poco::Net::HTTPServerParams::Ptr param = new Poco::Net::HTTPServerParams;
            param->setServerName(config().getString("nise.server.name", "nise"));
            param->setMaxThreads(config().getInt("nise.server.threads", 1));
            param->setKeepAlive(config().getBool("nise.server.keepalive", false));
            Poco::Net::ServerSocket svs(config().getInt("nise.server.port", 80));
            Poco::Net::HTTPServer srv(new RequestHandlerFactory,
                              svs, param);

            dropPrivilege();

            Log::system().information("Server is up.");

            BackgroundFetcher bg;
            Poco::Thread thread;
            thread.setPriority(Poco::Thread::PRIO_LOWEST);
            thread.start(bg);

            srv.start();
            waitForTerminationRequest();
            Log::system().information("Shutting down...");
            srv.stop();

            bg.stop();
            thread.join();

            Demo::cleanup();
            DynamicContent::cleanup();
            StaticContent::cleanup();
            SessionCache::cleanup();
            RetrievalCache::cleanup();
            Expansion::cleanup();
            SketchDB::cleanup();
            ImageDB::cleanup();
            Log::system().information("Server is down.");
        }
        return Poco::Util::Application::EXIT_OK;
    }
    
private:
    bool _helpRequested;
};


std::string StringToHex (const std::string &str) {
    static const char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(str.size()*2);
    BOOST_FOREACH(char cc, str) {
        unsigned c(cc);
        result += digits[(c >> 4) & 0xF];
        result += digits[c & 0xF];
    }
    return result;
}

static char hex2int (char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw WebInputException("bad key");
    return 0;
}

std::string HexToString (const std::string &hex) {
    if (hex.size() % 2) {
        throw WebInputException("bad key");
    }
    std::string result;
    result.reserve(hex.size() / 2);
    unsigned i = 0;
    while (i + 1 < hex.size()) {
        char c1 = hex2int(hex[i]);
        char c2 = hex2int(hex[i+1]);
        result.push_back((c1 << 4) + c2);
        i += 2;
    }
    return result;
}

}

POCO_SERVER_MAIN(nise::Server)

