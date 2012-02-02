#include <fstream>
#include <iostream>
#include <vector>
#include <boost/assert.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/Process.h>
#include "../common/nise.h"

namespace po = boost::program_options; 

int main (int argc, char *argv[]) {
    std::string input;
    std::string output;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input", po::value(&input), "")
    ("output", po::value(&output), "")
    ;

    po::positional_options_description p;
    p.add("input", 1).add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0)) {
        std::cerr << desc;
        return 1;
    }

    std::vector<nise::ImageID> ids;

    {
        std::ifstream is(input.c_str(), std::ios::binary);
        nise::ReadVector<nise::ImageID>(is, &ids);
    }

    nise::Environment env;
    Poco::Pipe inPipe;
    Poco::ProcessHandle sub(Poco::Process::launch("hadoop",
                                {"jar", env.jar_path(), "nise.Import", 
                                "-conf", "io.seqfile.compression.type=NONE",
                                output},
                                &inPipe, 0, 0));
    Poco::PipeOutputStream java_in(inPipe);

    for (uint32_t i = 0; i < ids.size(); ++i) {
        nise::WriteStringJava(java_in, nise::EncodeUint32(i));
        std::ostringstream os(std::ios::binary);
        nise::Signature::MAPPING.write(os);
        os.write((const char *)&ids[i], sizeof(ids[i]));
        nise::WriteStringJava(java_in, os.str());
    }
    java_in.flush();

    inPipe.close(Poco::Pipe::CLOSE_WRITE);
    sub.wait();

    return 0;
}

