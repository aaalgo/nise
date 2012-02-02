#include <sstream>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <hadoop/Pipes.hh>
#include <hadoop/TemplateFactory.hh>
#include "../common/nise.h"
#include "hadoop.h"

class GroupMap: public HadoopPipes::Mapper {
public:

  GroupMap(HadoopPipes::TaskContext& context) {
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
      record.regions.clear();
      record.features.clear();
      uint32_t container = nise::ContainerID(id);
      std::ostringstream ss(std::ios::binary);
      nise::WriteUint32(ss, id);
      record.write(ss);
      context.emit(nise::EncodeUint32Java(container), ss.str());
  }
};

// reduce is not used
class GroupReduce: public HadoopPipes::Reducer {
public:
  GroupReduce(HadoopPipes::TaskContext& context) {
  }

  void reduce(HadoopPipes::ReduceContext& context) {
      std::ostringstream ss(std::ios::binary);
      uint32_t count = 0;
      uint32_t size = 0;
      uint32_t key = nise::ParseUint32Java(context.getInputKey());
      nise::Signature::CONTAINER.write(ss);
      nise::WriteUint32(ss, key);
      std::stringstream::streampos pos = ss.tellp();
      nise::WriteUint32(ss, size);
      nise::WriteUint32(ss, count);
      //context.emit(nise::EncodeUint32(id), data);
      while (context.nextValue()) {
          const std::string &v = context.getInputValue();
          ss.write(&v[0], v.size());
          ++count;
      }
      size = ss.tellp();
      ss.seekp(pos);
      nise::WriteUint32(ss, size);
      nise::WriteUint32(ss, count);
      context.emit(std::string(), ss.str());
  }
};

class GroupPartitioner: public HadoopPipes::Partitioner {
    uint32_t total;
public:
    GroupPartitioner(HadoopPipes::TaskContext& context) {
        const HadoopPipes::JobConf* job = context.getJobConf();
        total = job->getInt("nise.containers.total");
    }

  virtual int partition(const std::string& key, int numOfReduces) {
      uint32_t v = nise::ParseUint32Java(key);
      return uint64_t(v) * numOfReduces / total;
  }
};

namespace po = boost::program_options; 

int main(int argc, char *argv[]) {
    if (nise::Environment::insideHadoop()) {
        return HadoopPipes::runTask(HadoopPipes::TemplateFactory<GroupMap, 
                              GroupReduce, GroupPartitioner>());
    }

    uint32_t total;
    std::string input;
    std::string output;
    std::vector<std::string> conf;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("conf,c", po::value(&conf), "hadoop configuration formated as NAME=VALUE")
    ("total", po::value(&total), "total number of images (STDOUT output of merge)")
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
            || (vm.count("output") == 0) 
            || (vm.count("total") == 0)) {
        std::cerr << "usage:" << std::endl;
        std::cerr << "\t" << argv[0] << " [--conf name=value ...] <--total #containers> <hdfs://input> <hdfs://output>" << std::endl;
        std::cerr << desc;
        return 1;
    }

    nise::Hadoop hadoop;

    hadoop.setProgram("group");
    hadoop.set("nise.containers.total", int((total + nise::CONTAINER_SIZE - 1) / nise::CONTAINER_SIZE));
    hadoop.useRawOutput();
    BOOST_FOREACH(const std::string &v, conf) {
        hadoop.add(v);
    }

    return hadoop.run(input, output);
}

