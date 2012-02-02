/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <hadoop/Pipes.hh>
#include <hadoop/TemplateFactory.hh>
#include <hadoop/StringUtils.hh>
#include <fbi.h>
#include "../common/nise.h"
#include "hadoop.h"


class GraphJoinMap: public HadoopPipes::Mapper {
public:

  GraphJoinMap(HadoopPipes::TaskContext& context) {
  }
  
  void map(HadoopPipes::MapContext& context) {
      std::vector<nise::HashEntry> v;
      {
          std::stringstream ss(context.getInputValue());
          nise::ReadVector<nise::HashEntry>(ss, &v);
          if (!ss) return;
      }
     
      fbi::Hamming hamming;
      if (v.size() > nise::MAX_HASH) {
          return;
      }
      for (unsigned i = 0; i < v.size(); ++i) {
          for (unsigned j = 0; j < i; ++j) {
              if (v[i].second == v[j].second) continue;
              if (hamming(v[i].first.sketch, v[j].first.sketch) < nise::SKETCH_DIST_OFFLINE) {
                  std::string v1(nise::EncodeUint32(v[i].second));
                  std::string v2(nise::EncodeUint32(v[j].second));
                  context.emit(v1, v2);
                  context.emit(v2, v1);
              }
          }
      }
  }
};

// reduce is not used
class GraphJoinReduce: public HadoopPipes::Reducer {
public:
  GraphJoinReduce(HadoopPipes::TaskContext& context) {
  }

  void reduce(HadoopPipes::ReduceContext& context) {
      std::string key = context.getInputKey();
      std::vector<uint32_t> ids;
      while (context.nextValue()) {
          ids.push_back(nise::ParseUint32(context.getInputValue()));
      }
      std::sort(ids.begin(), ids.end());
      ids.resize(std::unique(ids.begin(), ids.end()) - ids.begin());
      std::ostringstream ss(std::ios::binary);;
      nise::WriteVector<uint32_t>(ss, ids);
      std::string value(ss.str());
      context.emit(key, value);
  }
};

namespace po = boost::program_options; 

int main(int argc, char *argv[]) {
    if (nise::Environment::insideHadoop()) {
        return HadoopPipes::runTask(HadoopPipes::TemplateFactory<GraphJoinMap, 
                              GraphJoinReduce>());
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

    hadoop.setProgram("graph-join");
    hadoop.useRawOutput();
    BOOST_FOREACH(const std::string &v, conf) {
        hadoop.add(v);
    }

    return hadoop.run(input, output);
}

