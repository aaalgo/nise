#include <sstream>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <hadoop/Pipes.hh>
#include <hadoop/TemplateFactory.hh>
#include <hadoop/StringUtils.hh>
#include "../common/nise.h"
#include "hadoop.h"

const std::string HASH = "nise.index.graph.hash";
const std::string HASH_TRIMMED = "TRIMMED";

void SketchSplit (const nise::Sketch sk, std::vector<uint64_t> *v) {
    BOOST_VERIFY(nise::SKETCH_DIST_OFFLINE == 3);
    BOOST_VERIFY(nise::SKETCH_BIT == 128);
    v->resize(3);
    uint32_t p1 = *reinterpret_cast<const uint32_t *>(&sk[0]);
    uint32_t p2 = *reinterpret_cast<const uint32_t *>(&sk[4]);
    uint32_t p3 = *reinterpret_cast<const uint32_t *>(&sk[8]);
    uint32_t left = *reinterpret_cast<const uint32_t *>(&sk[12]);
    v->at(0) = (((uint64_t(left & ((1 << 11) - 1)) << 32) + p1) << 2) + 0;
    left = left >> 11;
    v->at(1) = (((uint64_t(left & ((1 << 11) - 1)) << 32) + p2) << 2) + 1;
    left = left >> 11;
    v->at(2) = (((uint64_t(left) << 32) + p3) << 2) + 2;
}

class GraphHashMap: public HadoopPipes::Mapper {
public:

  GraphHashMap(HadoopPipes::TaskContext& context) {
  }
  
  void map(HadoopPipes::MapContext& context) {
      nise::ImageID id = nise::ParseUint32(context.getInputKey());
      nise::Record record;
      {
          std::stringstream ss(context.getInputValue());
          nise::Signature::RECORD.check(ss);
          record.readFields(ss);
          if (!ss) return;
      }
     
      BOOST_FOREACH(const nise::Feature &f, record.features) {
          std::vector<uint64_t> v;
          SketchSplit(f.sketch, &v);
          nise::HashEntry e;
          e.first = f;
          e.second = id;
          std::ostringstream ss(std::ios::binary);
          ss.write((const char *)&e, sizeof(e));
          std::string value(ss.str());
          BOOST_FOREACH(uint64_t u, v) {
            context.emit(nise::EncodeUint64(u), value);
          }
      }
  }
};

// reduce is not used
class GraphHashReduce: public HadoopPipes::Reducer {
  HadoopPipes::TaskContext::Counter* trimmed;
public:
  GraphHashReduce(HadoopPipes::TaskContext& context) {
    trimmed = context.getCounter(HASH, HASH_TRIMMED);
  }

  void reduce(HadoopPipes::ReduceContext& context) {
      std::vector<nise::HashEntry> entries;
      std::string key = context.getInputKey();
      while (context.nextValue()) {
          if (entries.size() > nise::MAX_HASH) {
              context.incrementCounter(trimmed, 1);
              break;
          }
          std::stringstream ss(context.getInputValue());
          nise::HashEntry e;
          ss.read((char *)&e, sizeof(e));
          if (ss) {
              entries.push_back(e);
          }
      }
      std::ostringstream ss(std::ios::binary);
      nise::WriteVector<nise::HashEntry>(ss, entries);
      context.emit(key, ss.str());
  }
};

namespace po = boost::program_options; 

int main(int argc, char *argv[]) {
    if (nise::Environment::insideHadoop()) {
        return HadoopPipes::runTask(HadoopPipes::TemplateFactory<GraphHashMap, 
                              GraphHashReduce>());
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

    hadoop.setProgram("graph-hash");
    BOOST_FOREACH(const std::string &v, conf) {
        hadoop.add(v);
    }

    return hadoop.run(input, output);
}

