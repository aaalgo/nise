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
#include <sstream>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <hadoop/Pipes.hh>
#include <hadoop/TemplateFactory.hh>
#include <hadoop/StringUtils.hh>
#include <fbi.h>
#include "../common/nise.h"
#include "hadoop.h"


// reduce is not used
class MyReduce: public HadoopPipes::Reducer {
public:
  MyReduce(HadoopPipes::TaskContext& context) {
  }

  void reduce(HadoopPipes::ReduceContext& context) {
      nise::ImageID id;
      bool id_read = false;
      nise::Record record;
      bool record_read = false;
      int cnt = 0;
      while (context.nextValue()) {
          std::stringstream ss(context.getInputValue());
          uint32_t sig = nise::ReadUint32(ss);
          if (nise::Signature::RECORD.check(sig)) {
              record.readFields(ss);
              record_read = true;
          }
          else if (nise::Signature::MAPPING.check(sig)) {
              id = nise::ReadUint32(ss);
              id_read = true;
          }
          else BOOST_VERIFY(0);
          ++cnt;
      }
      BOOST_VERIFY(cnt == 2);
      BOOST_VERIFY(id_read && record_read);
      std::ostringstream ss(std::ios::binary);
      nise::Signature::RECORD.write(ss);
      record.write(ss);
      context.emit(nise::EncodeUint32(id), ss.str());
  }
};

namespace po = boost::program_options; 

int main(int argc, char *argv[]) {
    if (nise::Environment::insideHadoop()) {
        return HadoopPipes::runTask(HadoopPipes::TemplateFactory<nise::DummyMapper, 
                              MyReduce>());
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

    hadoop.setProgram("mapid");
    hadoop.useIdentityMapper();
    BOOST_FOREACH(const std::string &v, conf) {
        hadoop.add(v);
    }

    return hadoop.run(input, output);
}

