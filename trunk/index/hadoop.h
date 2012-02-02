#ifndef WDONG_NISE_HADOOP
#define WDONG_NISE_HADOOP

#include <hadoop/Pipes.hh>
#include <hadoop/TemplateFactory.hh>
#include <Poco/TemporaryFile.h>

namespace nise {
    class DummyMapper: public HadoopPipes::Mapper {
    public:
      DummyMapper(HadoopPipes::TaskContext& context) {
          BOOST_VERIFY(0);
      }
      void map(HadoopPipes::MapContext& context) {
      }
    };

    class DummyReducer: public HadoopPipes::Reducer {
    public:
      DummyReducer (HadoopPipes::TaskContext& context) {
          BOOST_VERIFY(0);
      }
      void reduce (HadoopPipes::ReduceContext& context) {
      }
    };

    class Hadoop {
        std::map<std::string, std::string> config;
        Environment env;
        static const bool new_api = false;

        void setDefaults () {
            if (new_api) {
                set("mapred.reducer.new-api", true);
                set("mapred.mapper.new-api", true);
                set("hadoop.pipes.java.recordreader", true);
                set("hadoop.pipes.java.recordwriter", true);
                set("mapreduce.map.output.key.class", "org.apache.hadoop.io.BytesWritable");
                set("mapreduce.map.output.value.class", "org.apache.hadoop.io.BytesWritable");
                set("mapreduce.job.output.key.class", "org.apache.hadoop.io.BytesWritable");
                set("mapreduce.job.output.value.class", "org.apache.hadoop.io.BytesWritable");
                set("mapreduce.job.inputformat.class", "org.apache.hadoop.mapreduce.lib.input.SequenceFileInputFormat");
                set("mapreduce.job.outputformat.class", "org.apache.hadoop.mapreduce.lib.output.SequenceFileOutputFormat");
                set("mapreduce.map.failures.maxpercent", 1);
                set("mapreduce.reduce.failures.maxpercent", 1);
                set("mapreduce.map.maxattempts", 2);
            }
            else {
                set("mapred.reducer.new-api", false);
                set("mapred.mapper.new-api", false);
                set("hadoop.pipes.java.recordreader", true);
                set("hadoop.pipes.java.recordwriter", true);
                set("mapred.mapoutput.key.class", "org.apache.hadoop.io.BytesWritable");
                set("mapred.mapoutput.value.class", "org.apache.hadoop.io.BytesWritable");
                set("mapred.output.key.class", "org.apache.hadoop.io.BytesWritable");
                set("mapred.output.value.class", "org.apache.hadoop.io.BytesWritable");
                set("mapred.input.format.class", "org.apache.hadoop.mapred.SequenceFileInputFormat");
                set("mapred.output.format.class", "org.apache.hadoop.mapred.SequenceFileOutputFormat");
                set("dfs.block.size", 134217728);
                set("mapred.max.map.failures.percent", 1);
                set("mapred.max.reduce.failures.percent", 1);
                set("mapred.map.max.attempts", 2);
            }
            setNumReduceTasks(500);
        }

        void writeConfig (std::ostream &os) {
            os << "<?xml version=\"1.0\"?>" << std::endl
               << "<configuration>" << std::endl;
            BOOST_FOREACH(auto &v, config) {
                os << "<property>" << std::endl
                    << "<name>" << v.first << "</name>" << std::endl
                    << "<value>" << v.second << "</value>" << std::endl
                    << "</property>" << std::endl;
            }
            os << "</configuration>\n";
        }
    public:
        Hadoop () {
            setDefaults();
        }

        void set (const std::string &key, const std::string &value) {
            config[key] = value;
        }

        void set (const std::string &key, const char *value) {
            config[key] = value;

        }

        void set (const std::string &key, int value) {
            config[key] = boost::lexical_cast<std::string>(value);
        }

        void set (const std::string &key, bool value) {
            config[key] = value ? "true" : "false";
        }

        void useRawOutput () {
            if (new_api) {
                set("mapreduce.job.jar", env.jar_path());
                set("mapreduce.job.outputformat.class", "nise.BinaryOutputFormat");
            }
            else {
                set("mapred.jar", env.jar_path());
                set("mapred.output.format.class", "nise.BinaryOutputFormatDeprecated");
            }
        }

        void useJavaMapper (const std::string &cls) {
            set("hadoop.pipes.java.mapper", true);
            if (new_api) {
                set("mapreduce.job.map.class", cls);
            }
            else {
                set("mapred.mapper.class", cls);
            }
        }

        void useIdentityMapper () {
            if (new_api) {
                set("mapreduce.job.jar", env.jar_path());
                useJavaMapper("nise.IdentityMapper");
            }
            else {
                useJavaMapper("org.apache.hadoop.mapred.lib.IdentityMapper");
            }
        }

        void useJavaReducer (const std::string &cls) {
            set("hadoop.pipes.java.reducer", true);
            if (new_api) {
                set("mapreduce.job.reduce.class", cls);
            }
            else {
                set("mapred.reducer.class", cls);
            }
        }

        void useIdentityReducer () {
            if (new_api) {
                set("mapreduce.job.jar", env.jar_path());
                useJavaReducer("nise.IdentityReducer");
            }
            else {
                useJavaReducer("org.apache.hadoop.mapred.lib.IdentityReducer");
            }
        }

        void setNumReduceTasks (int v) {
            if (new_api) {
                set("mapreduce.job.reduces", v);
            }
            else {
                set("mapred.reduce.tasks", v);
            }
        }

        void setProgram (const std::string &program) {
            set("hadoop.pipes.executable",
                    env.hadoopHome() + "/bin/" + program + "#" + program);
        }

        void add (const std::string &conf) {
            size_t off = conf.find('=');
            BOOST_VERIFY(off != std::string::npos);
            set(conf.substr(0, off), conf.substr(off + 1));
        }

        int run (const std::string &input, const std::string &output) {
            Poco::TemporaryFile temp;
            /*
            std::cerr << temp.path() << std::endl;
            temp.keep();
            */

            {
                std::ofstream os(temp.path().c_str());
                writeConfig(os);
            }

            std::string cmd("mapred pipes -conf " + temp.path() + " -input " + input + " -output " + output);
            return system(cmd.c_str());
        }
    };
}


#endif

