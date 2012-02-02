#include <sstream>
#include <boost/program_options.hpp>
#include "../common/nise.h"
#include "../image/extractor.h"
#include "hadoop.h"

const std::string MERGE = "nise.index.import.nutch";
const std::string MERGE_BAD = "BAD";

// identical map, not used
class MyMapper: public HadoopPipes::Mapper {
  HadoopPipes::TaskContext::Counter* bad;
  nise::Extractor xtor;

public:

  MyMapper(HadoopPipes::TaskContext& context) {
    bad = context.getCounter(MERGE, MERGE_BAD);
  }

  void map (HadoopPipes::MapContext& context) {
      const std::string &url = context.getInputKey();
      const std::string &content = context.getInputValue();

      nise::Record record;

      xtor.extract(content, &record, false);

      if (record.features.empty()) {
          context.incrementCounter(bad, 1);
      }
      else {
        record.sources.resize(1);
        record.sources.back().url = url;
        std::ostringstream ss(std::ios::binary);
        nise::Signature::RECORD.write(ss);
        record.write(ss);
        context.emit(record.checksum, ss.str());
      }
  }
};

namespace po = boost::program_options; 

int main(int argc, char *argv[]) {
    if (nise::Environment::insideHadoop()) {
        return HadoopPipes::runTask(HadoopPipes::TemplateFactory<MyMapper, 
                              nise::DummyReducer>());
    }

    std::string input;
    std::string output;
    std::vector<std::string> conf;

    po::options_description desc;
    desc.add_options()
    ("help,h", "produce help message.")
    ("conf,c", po::value(&conf), "hadoop configuration formated as NAME=VALUE")
    ("input", po::value(&input), "hadoop input")
    ("output", po::value(&output), "hadoop output")
    ;

    po::positional_options_description p;
    p.add("input", 1).add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0)) {
        std::cerr << "usage:" << std::endl;
        std::cerr << "\t" << argv[0] << " [--conf name=value ...] <hdfs://input> <hdfs://output>" << std::endl;
        std::cerr << desc;
        return 1;
    }

    nise::Hadoop hadoop;

    hadoop.setProgram("import-nutch");
    hadoop.setNumReduceTasks(0);
    BOOST_FOREACH(const std::string &v, conf) {
        hadoop.add(v);
    }

    return hadoop.run(input, output);
}

