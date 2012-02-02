#include <sstream>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <hadoop/Pipes.hh>
#include <hadoop/TemplateFactory.hh>
#include <fbi.h>
#include "../common/nise.h"
#include "hadoop.h"



class SketchIndexMap: public HadoopPipes::Mapper {
  unsigned offset;
public:

  SketchIndexMap(HadoopPipes::TaskContext& context) {
        const HadoopPipes::JobConf* job = context.getJobConf();
        offset = job->getInt("nise.sketch.index.offset");
  }
  
  void map(HadoopPipes::MapContext& context) {
      const std::string &value = context.getInputKey();
      nise::Record record;
      {
          std::stringstream ss(context.getInputValue());
          nise::Signature::RECORD.check(ss);
          record.readFields(ss);
          if (!ss) return;
      }
      BOOST_FOREACH(const nise::Feature &f, record.features) {
          nise::Sketch rot;
          fbi::Rotate(f.sketch, offset, rot);
          std::string key(reinterpret_cast<const char*>(rot), nise::SKETCH_SIZE);
          context.emit(key, value);
      }
  }
};

// reduce is not used
class SketchIndexReduce: public HadoopPipes::Reducer {
  unsigned offset;
public:
  SketchIndexReduce(HadoopPipes::TaskContext& context) {
        const HadoopPipes::JobConf* job = context.getJobConf();
        offset = job->getInt("nise.sketch.index.offset");
  }

  void reduce(HadoopPipes::ReduceContext& context) {
      const std::string &key = context.getInputKey();
      std::string key_unrot;
      key_unrot.resize(nise::SKETCH_SIZE);
      fbi::Unrotate(reinterpret_cast<const nise::Chunk *>(&key[0]),
              offset, reinterpret_cast<nise::Chunk *>(&key_unrot[0]));
      while (context.nextValue()) {
          const std::string &value = context.getInputValue();
          context.emit(key_unrot, value);
      }
  }
};

class SketchIndexPartitioner: public HadoopPipes::Partitioner {
    static const uint32_t total = 0x10000;
public:
    SketchIndexPartitioner(HadoopPipes::TaskContext& context) {
    }

    virtual int partition(const std::string& key, int numOfReduces) {
        uint32_t v = nise::ParseUint16Java(key);
        return v * numOfReduces / total;
    }
};

namespace po = boost::program_options; 

int main(int argc, char *argv[]) {
    if (nise::Environment::insideHadoop()) {
        return HadoopPipes::runTask(HadoopPipes::TemplateFactory<SketchIndexMap, 
                              SketchIndexReduce, SketchIndexPartitioner>());
    }

    int offset;
    std::string input;
    std::string output;
    std::vector<std::string> conf;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("conf,c", po::value(&conf), "hadoop configuration formated as NAME=VALUE")
    ("offset", po::value(&offset)->default_value(0), "sketch offset, from 0 to 127")
    ("input", po::value(&input), "hadoop input")
    ("output", po::value(&output), "hadoop output")
    ;

    po::positional_options_description p;
    p.add("input", 1).add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") 
            || (vm.count("input") == 0) 
            || (vm.count("output") == 0)) {
        std::cerr << "usage:" << std::endl;
        std::cerr << "\t" << argv[0] << " [--conf name=value ...] <--offset offset> <hdfs://input> <hdfs://output>" << std::endl;
        std::cerr << desc;
        return 1;
    }

    nise::Hadoop hadoop;

    hadoop.setProgram("sketch-index");
    hadoop.set("nise.sketch.index.offset", offset);
    hadoop.useRawOutput();
    BOOST_FOREACH(const std::string &v, conf) {
        hadoop.add(v);
    }

    return hadoop.run(input, output);
}

