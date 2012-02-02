#include <sstream>
#include <boost/program_options.hpp>
#include "../common/nise.h"
#include "hadoop.h"

const std::string MERGE = "nise.index.merge";
const std::string MERGE_TRIMMED = "TRIMMED";
const std::string MERGE_BAD = "BAD";

// identical map, not used
class MergeReducer: public HadoopPipes::Reducer {
public:
  // # sources overflow
  HadoopPipes::TaskContext::Counter* trimmed;
  // bad format
  HadoopPipes::TaskContext::Counter* bad;

  MergeReducer(HadoopPipes::TaskContext& context) {
    trimmed = context.getCounter(MERGE, MERGE_TRIMMED);
    bad = context.getCounter(MERGE, MERGE_BAD);
  }

  void reduce(HadoopPipes::ReduceContext& context) {
    bool first = true;
    nise::Record record;
    std::string key = context.getInputKey();
    while (context.nextValue()) {
      std::stringstream ss(context.getInputValue());
      if (first) {
          nise::Signature::RECORD.check(ss);
          record.readFields(ss);
          if (ss) {
              first = false;
              continue;
          }
      }
      else {
          if (record.sources.size() > nise::MAX_SOURCES) {
              context.incrementCounter(trimmed, 1);
              break;
          }
          nise::Record cur;
          nise::Signature::RECORD.check(ss);
          cur.readFields(ss);
          if (ss && (cur.sources.size() > 0)) {
              record.sources.push_back(cur.sources[0]);
              continue;
          }
      }
      context.incrementCounter(bad, 1);
    }
    if (!first) {
        std::ostringstream ss(std::ios::binary);
        nise::Signature::RECORD.write(ss);
        record.write(ss);
        context.emit(key, ss.str());
    }
  }
};

namespace po = boost::program_options; 

int main(int argc, char **argv) {
    if (nise::Environment::insideHadoop()) {
        return HadoopPipes::runTask(HadoopPipes::TemplateFactory<nise::DummyMapper, 
                    MergeReducer>());
    }

    std::string input;
    std::string output;
    std::vector<std::string> conf;

    po::options_description desc("Allowed options");
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

    hadoop.setProgram("merge");
    hadoop.useIdentityMapper();
    BOOST_FOREACH(const std::string &v, conf) {
        hadoop.add(v);
    }

    hadoop.run(input, output);
    return 0;
}

