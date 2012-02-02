package nise;

import java.io.IOException;
import java.io.PrintStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.ArrayList;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.conf.Configured;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.PathFilter;
import org.apache.hadoop.io.Writable;
import org.apache.hadoop.io.IntWritable;
import org.apache.hadoop.io.BytesWritable;
import org.apache.hadoop.io.SequenceFile;
import org.apache.hadoop.util.Tool;
import org.apache.hadoop.util.ToolRunner;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Cluster;
import org.apache.hadoop.mapreduce.Partitioner;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.SequenceFileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.SequenceFileOutputFormat;

public class Number extends Configured implements Tool {

    static final String TEMP_KEY = "nise.number.counts";

    static class Prefix {

        public static final int MAX = 1024;

        public static int get (BytesWritable bw) {
            int ret = 0;
            byte[] bytes = bw.getBytes();
            if (bytes.length >= 1) {
                ret *= 256;
                ret += bytes[0] - Byte.MIN_VALUE;
            }
            if (bytes.length >= 2) {
                ret *= 256;
                ret += bytes[1] - Byte.MIN_VALUE;
            }
            return ret % MAX;
        }

    }

    static class MyPartitioner extends Partitioner<IntWritable, Writable> {
        public int getPartition(IntWritable key, Writable value, int numPartitions) {
            return key.get() * numPartitions / Prefix.MAX;
        }
    }

    static class Mapper1 extends
        Mapper<BytesWritable, Writable, IntWritable, IntWritable> 
    {
        public void map(BytesWritable key, Writable value, Context context)
            throws IOException, InterruptedException
        {
            context.write(new IntWritable(Prefix.get(key)), new IntWritable(1));
        }
    }

    static class Reducer1 extends
        Reducer<IntWritable, IntWritable, IntWritable, IntWritable>
    {
        public void reduce(IntWritable key, Iterable<IntWritable> values, Context context) 
            throws IOException, InterruptedException
        {
            int count = 0;
            for (IntWritable v: values) {
                count += v.get();
            }
            context.write(key, new IntWritable(count));
        }
    }


    static class Mapper2 extends
         Mapper<BytesWritable, BytesWritable, IntWritable, BytesWritable>
    {
        public void map (BytesWritable key, BytesWritable value, Context context)
            throws IOException, InterruptedException
        {
            context.write(new IntWritable(Prefix.get(key)), value);
        }
    }

    static class Reducer2 extends
        Reducer<IntWritable, BytesWritable, BytesWritable, BytesWritable>
    {
        int[] start;
        public void reduce(IntWritable key, Iterable<BytesWritable> values, Context context)
            throws IOException, InterruptedException
        {
            int id = start[key.get()];
            if (key.get() == 0) {
                if (id != 0) {
                    throw new RuntimeException("xxx");
                }
            }
            for (BytesWritable v: values) {
                byte[] keyBuffer = new byte[4];
                ByteBuffer bb = ByteBuffer.wrap(keyBuffer);
                bb.order(ByteOrder.nativeOrder());
                bb.putInt(id);
                context.write(new BytesWritable(keyBuffer), v);
                id++;
            }
            if (id != start[(int)key.get() + 1]) {
                throw new RuntimeException("BAD COUNT");
            }
        }

        public void setup (Context context) {
            Configuration conf = context.getConfiguration();
            String temp = conf.get(TEMP_KEY);
            if (temp == null) {
                throw new RuntimeException("no temp file");
            }

            try {
                FileSystem fs = FileSystem.get(conf);
                FSDataInputStream is = fs.open(new Path(temp));
                start = new int[is.readInt()];
                for (int i = 0; i < start.length; i++) {
                    start[i] = is.readInt();
                }
                is.close();
            }
            catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

    }

    static class MyPathFilter implements org.apache.hadoop.fs.PathFilter {
        public boolean accept (Path p) {
            if (p.getName().startsWith("_")) return false;
            if (p.getName().startsWith(".")) return false;
            return true;
        }
    }

    public int run (String[] args) throws Exception {
        if (args.length < 2) {
            System.err.println("Number in out");
            return -1;
        }
        int part = 1000;
        if (args.length >= 3) {
            part = Integer.parseInt(args[2]);
        }

        Path in = new Path(args[0]);
        Path out =  new Path(args[1]);

        String temp1 = "_tmp/bang.number.stage1";
        String temp2 = "_tmp/bang.number.stage2";

        Configuration conf = new Configuration();
        Cluster cluster = new Cluster(conf);
        FileSystem fs = FileSystem.get(conf);
        fs.delete(new Path(temp1), true);

        //System.err.println("Stage 1...");
        {
            Job stage1 = Job.getInstance(cluster, conf);
            stage1.setJarByClass(Number.class);
            stage1.setMapperClass(Mapper1.class);
            stage1.setCombinerClass(Reducer1.class);
            stage1.setReducerClass(Reducer1.class);
            stage1.setPartitionerClass(MyPartitioner.class);
            stage1.setOutputKeyClass(IntWritable.class);
            stage1.setOutputValueClass(IntWritable.class);
            stage1.setInputFormatClass(SequenceFileInputFormat.class);
            stage1.setOutputFormatClass(SequenceFileOutputFormat.class);
            stage1.setNumReduceTasks(part);
            SequenceFileInputFormat.addInputPath(stage1, in);
            SequenceFileOutputFormat.setOutputPath(stage1, new Path(temp1));
            stage1.waitForCompletion(true);
        }

        //System.err.println("Stage 2...");
        ArrayList<IntWritable> al = new ArrayList<IntWritable>();
        IntWritable key = new IntWritable();
        IntWritable value = new IntWritable();
        FileStatus[] fstats = fs.listStatus(new Path(temp1), new MyPathFilter());
        for (FileStatus fstat: fstats) {
            //System.err.println(fstat.getPath());
            SequenceFile.Reader reader = new SequenceFile.Reader(fs, fstat.getPath(), conf);
            while (reader.next(key, value)) {
                // System.err.println("KEY: " + key + " VALUE: " + value);
                if (key.get() < al.size()) {
                    throw new RuntimeException("logic error");
                }
                if (key.get() >= Prefix.MAX) {
                    throw new RuntimeException("range error");
                }
                while (al.size() < key.get()) {
                    al.add(new IntWritable(0));
                }
                al.add(value);
                value = new IntWritable();
            }
        }

        fs.delete(new Path(temp2), true);

        int sum = 0;
        FSDataOutputStream os = fs.create(new Path(temp2), false);
        os.writeInt(al.size() + 1);
        for (IntWritable l: al) {
            os.writeInt(sum);
            sum += l.get();
        }
        os.writeInt(sum);
        os.close();
        System.out.println(sum);


        {
            conf.set(TEMP_KEY, temp2);

            //Job stage2 = new Job(conf, "Number.Stage2");
            Job stage2 = Job.getInstance(cluster, conf);
            stage2.setJarByClass(Number.class);
            stage2.setMapperClass(Mapper2.class);
            stage2.setReducerClass(Reducer2.class);
            stage2.setMapOutputKeyClass(IntWritable.class);
            stage2.setMapOutputValueClass(BytesWritable.class);
            stage2.setOutputKeyClass(BytesWritable.class);
            stage2.setOutputValueClass(BytesWritable.class);
            stage2.setInputFormatClass(SequenceFileInputFormat.class);
            stage2.setOutputFormatClass(SequenceFileOutputFormat.class);
            stage2.setPartitionerClass(MyPartitioner.class);
            stage2.setNumReduceTasks(part);
            SequenceFileInputFormat.addInputPath(stage2, in);
            SequenceFileOutputFormat.setOutputPath(stage2, out);
            stage2.waitForCompletion(true);
        }

        PrintStream str = new PrintStream(fs.create(new Path(out + ".size"), false));
        str.println(sum);
        str.close();
        
        return 0;
    }

    public static void main(String[] args) throws Exception {
        int res = ToolRunner.run(new Number(), args);
        System.exit(res);
    }
}

