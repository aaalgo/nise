package nise;

import java.io.IOException;

import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.FSDataOutputStream;

import org.apache.hadoop.io.BytesWritable;

import org.apache.hadoop.mapred.JobConf;
import org.apache.hadoop.mapred.RecordWriter;
import org.apache.hadoop.mapred.Reporter;
import org.apache.hadoop.util.Progressable;
import org.apache.hadoop.mapred.FileOutputFormat;

public class BinaryOutputFormatDeprecated extends FileOutputFormat<BytesWritable, BytesWritable> {

  public RecordWriter<BytesWritable, BytesWritable> 
         getRecordWriter(FileSystem ignored, JobConf job,
                        String name, Progressable progress
                         ) throws IOException {
    
    // get the path of the temporary output file 
    Path file = FileOutputFormat.getTaskOutputPath(job, name);
    FileSystem fs = file.getFileSystem(job);
    final FSDataOutputStream os = fs.create(file);

    return new RecordWriter<BytesWritable, BytesWritable>() {

        public void write(BytesWritable key, BytesWritable value)
          throws IOException {
          os.write(key.getBytes(), 0, key.getLength());
          os.write(value.getBytes(), 0, value.getLength());
        }

        public void close(Reporter context) throws IOException { 
          os.close();
        }
      };
  }
}

