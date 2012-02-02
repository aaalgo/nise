#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/Process.h>
#include <Poco/StreamCopier.h>
#include <Poco/SHA1Engine.h>
#include <Poco/File.h>
#include <fbi.h>
#include "nise.h"
#include <char_bit_cnt.inc>

namespace nise {

    ConstantRangeCheck ConstantRangeCheck::run;

    Signature Signature::IMAGE("imag");
    Signature Signature::FEATURES("feat");
    Signature Signature::RECORD("reco");
    Signature Signature::MAPPING("mapp");
    Signature Signature::CONTAINER("cont");

    Extension2Mime Extension2Mime::instance;

    Environment::Environment (): home_(getenv("NISE_HOME")), hadoop_home_(getenv("NISE_HADOOP_HOME")) {
        BOOST_VERIFY(!home_.empty());
        BOOST_VERIFY(!hadoop_home_.empty());
        Poco::File file(home_);
        BOOST_VERIFY(file.exists());
    }

    std::string Environment::jar_path () const {
            std::string jar_path = home_ + "/java/nise.jar";
            Poco::File file(jar_path);
            BOOST_VERIFY(file.exists());
            return jar_path;
    }


    void Download (const std::string &url, std::string *file) {
            Poco::Pipe inPipe;
            std::vector<std::string> args;
            args.push_back("--output-document=-");
            args.push_back("--tries=1");
            args.push_back(url);
            Poco::ProcessHandle sub(Poco::Process::launch("wget",
                        args, 0, &inPipe, 0));
            Poco::PipeInputStream input(inPipe);
            Poco::StreamCopier::copyToString(input, *file);
            inPipe.close(Poco::Pipe::CLOSE_READ);
            sub.wait();
    }

    void Checksum (const std::string &data, std::string *checksum) {
        Poco::SHA1Engine sha1;
        sha1.update(data);
        const Poco::SHA1Engine::Digest &digest = sha1.digest();
        checksum->resize(digest.size());
        copy(digest.begin(), digest.end(), &checksum->at(0));
    }
}

