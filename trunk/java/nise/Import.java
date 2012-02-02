package nise;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.conf.Configured;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.SequenceFile;
import org.apache.hadoop.io.BytesWritable;
import org.apache.hadoop.util.Tool;
import org.apache.hadoop.util.ToolRunner;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.EOFException;

public class Import extends Configured implements Tool {

    public int run(String[] args) throws Exception {
        if (args.length < 1) {
            System.err.println("Import output");
            return 1;
        }
        try {
            String output = args[0];
            Configuration conf = new Configuration();
            FileSystem fs = FileSystem.get(conf);
            SequenceFile.Writer writer = SequenceFile.createWriter(fs, conf, new Path(args[0]), BytesWritable.class, BytesWritable.class);
            DataInputStream input = new DataInputStream(System.in);
            BytesWritable key = new BytesWritable();
            BytesWritable value = new BytesWritable();
            int count = 0;
            while (true) {
                try {
                    key.readFields(input);
                    value.readFields(input);
                    writer.append(key, value);
                    count++;
                }
                catch (EOFException e) {
                    break;
                }
            }
            writer.close();
            System.err.println(count + " records imported.");
        }
        catch (Exception e) {
            System.err.println(e);
            return 2;
        }
        return 0;
    }

    public static void main(String[] args) throws Exception {
        int res = ToolRunner.run(new Import(), args);
        System.exit(res);
    }
}
