#ifndef WDONG_NISE_EXTRACTOR
#define WDONG_NISE_EXTRACTOR

#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/Process.h>

namespace nise {
    class Extractor {
        Environment env;
        Poco::Pipe inPipe, outPipe;
        Poco::ProcessHandle sub;
        Poco::PipeInputStream input; 
        Poco::PipeOutputStream output;
    public:
        Extractor (const std::vector<std::string> &args = std::vector<std::string>())
            : sub(Poco::Process::launch(env.home() + "/bin/extract",
                    args, &outPipe, &inPipe, 0)),
              input(inPipe),
              output(outPipe)
        {
            BOOST_VERIFY(input);
            BOOST_VERIFY(output);
        }

        void extract (const std::string &image, Record *record) {
            Signature::IMAGE.write(output);
            WriteString(output, image);
            output.flush();
            BOOST_VERIFY(output);
            Signature::RECORD.check(input);
            BOOST_VERIFY(input);
            record->readFields(input);
            BOOST_VERIFY(input);
        }

        ~Extractor () {
            inPipe.close(Poco::Pipe::CLOSE_READ);
            outPipe.close(Poco::Pipe::CLOSE_WRITE);
            sub.wait();
        }
    };
}

#endif
