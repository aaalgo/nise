package nise;

import java.io.IOException;

import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.FSDataOutputStream;

import org.apache.hadoop.io.BytesWritable;

import org.apache.hadoop.mapreduce.RecordWriter;
import org.apache.hadoop.mapreduce.TaskAttemptContext;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;
import org.apache.hadoop.conf.Configuration;

public class BinaryOutputFormat extends FileOutputFormat<BytesWritable, BytesWritable> {

  public RecordWriter<BytesWritable, BytesWritable> 
         getRecordWriter(TaskAttemptContext context
                         ) throws IOException, InterruptedException {
    Configuration conf = context.getConfiguration();
    
    // get the path of the temporary output file 
    Path file = getDefaultWorkFile(context, "");
    FileSystem fs = file.getFileSystem(conf);
    final FSDataOutputStream os = fs.create(file);

    return new RecordWriter<BytesWritable, BytesWritable>() {

        public void write(BytesWritable key, BytesWritable value)
          throws IOException {
          os.write(key.getBytes());
          os.write(value.getBytes());
        }

        public void close(TaskAttemptContext context) throws IOException { 
          os.close();
        }
      };
  }
}

