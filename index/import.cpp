#include <boost/program_options.hpp>
#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/Process.h>
#include "../common/nise.h"
#include "../image/extractor.h"

using namespace nise;

namespace po = boost::program_options; 

void run (std::istream &is, std::ostream &os, std::ostream &es) 
{
    std::string path, url;

    Extractor *xtor = new Extractor();

    while (is >> path >> url) {
        std::string bin;
        ReadFile(path, &bin);
        Record record;

        es << path << ' '  << url << std::endl;

        xtor->extract(bin, &record, false);
        if (record.features.size() == 0) continue;
        record.regions.clear();
        record.sources.resize(1);
        record.sources[0].url = path;
        record.sources[0].parentUrl = url;

        std::ostringstream ss(std::ios::binary);
        Signature::RECORD.write(ss);
        record.write(ss);

        WriteStringJava(os, record.checksum);
        WriteStringJava(os, ss.str());
    }

    delete xtor;
}

int main (int argc, char *argv[]) {

    std::string input;
    std::string output;
    std::string log;
    std::vector<std::string> conf;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input,I", po::value(&input), "input file")
    ("output,O", po::value(&output), "hadoop output")
    ("log,E", po::value(&log), "log file")
    ("local", "output is a local file")
    ;

    po::positional_options_description p;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || ((vm.count("local") == 0) && (vm.count("output") == 0))) {
        std::cerr << desc;
        return 1;
    }

    std::ifstream in;
    if (!input.empty()) {
        in.open(input.c_str());
        BOOST_VERIFY(in);
    }

    std::istream &cin = input.empty() ? std::cin : in;

    std::ofstream err;
    if (!log.empty()) {
        err.open(log.c_str());
        BOOST_VERIFY(err);
    }

    std::ostream &cerr = log.empty() ? std::cerr : err;

    if (vm.count("local")) {
        std::ofstream out;
        if (!output.empty()) {
            out.open(output.c_str(), std::ios::binary);
            BOOST_VERIFY(out);
        }
        std::ostream &cout = output.empty() ? std::cout : out;
        run(cin, cout, cerr);
        return 0;
    }

    Environment env;

    std::vector<std::string> args;
    args.push_back("jar");
    args.push_back(env.jar_path());
    args.push_back("nise.Import");
    args.push_back(output);
    Poco::Pipe inPipe;
    Poco::ProcessHandle sub(Poco::Process::launch("hadoop", args, &inPipe, 0, 0));
    Poco::PipeOutputStream java_in(inPipe); 
    run(cin, java_in, cerr);
    java_in.flush();
    inPipe.close(Poco::Pipe::CLOSE_WRITE);
    sub.wait();

    return 0;

}

