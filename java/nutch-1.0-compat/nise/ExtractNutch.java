package nise;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.BytesWritable;
import org.apache.hadoop.io.SequenceFile;
import org.apache.hadoop.io.Writable;
import org.apache.hadoop.mapred.FileInputFormat;
import org.apache.hadoop.mapred.FileOutputFormat;
import org.apache.hadoop.mapred.JobClient;
import org.apache.hadoop.mapred.JobConf;
import org.apache.hadoop.mapred.Mapper;
import org.apache.hadoop.mapred.lib.IdentityReducer;
import org.apache.hadoop.mapred.OutputCollector;
import org.apache.hadoop.mapred.Reporter;
import org.apache.hadoop.mapred.SequenceFileInputFormat;
import org.apache.hadoop.mapred.SequenceFileOutputFormat;

import org.apache.nutch.protocol.Content;

import java.io.IOException;

import java.util.ArrayList;


public class ExtractNutch implements Mapper<Writable, Content, BytesWritable, BytesWritable>
{
    public void configure(JobConf conf) {
    }

    public void close() throws IOException {
    }

    public void map(Writable key, Content value,
        OutputCollector<BytesWritable, BytesWritable> output, Reporter reporter)
        throws IOException {
        Content content = (Content) value;

        if (content.getContentType().toLowerCase().startsWith("image/jpeg")) {
            try {
                //MessageDigest md = MessageDigest.getInstance("SHA-1");
                output.collect(new BytesWritable(content.getUrl().getBytes()),
                            new BytesWritable(content.getContent()));
            } catch (Exception e) {
            }
        }
    }

    public static void extract(Path out, Path[] segs) throws Exception {
        JobConf job = new JobConf(ExtractNutch.class);
        FileSystem fs = FileSystem.get(job);

        // prepare the minimal common set of input dirs
        for (int i = 0; i < segs.length; i++) {
            Path cDir = new Path(segs[i], Content.DIR_NAME);

            FileStatus st = fs.getFileStatus(segs[i]);
            if (st.isDir() && fs.exists(cDir)) {
                FileInputFormat.addInputPath(job, cDir);
            }
        }

        FileOutputFormat.setOutputPath(job, out);
        job.setInputFormat(SequenceFileInputFormat.class);
        job.setOutputFormat(SequenceFileOutputFormat.class);
        job.setMapperClass(ExtractNutch.class);
        job.setReducerClass(IdentityReducer.class);
        job.setOutputKeyClass(BytesWritable.class);
        job.setOutputValueClass(BytesWritable.class);
        job.setNumReduceTasks(462);
        JobClient.runJob(job);
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 2) {
            System.err.println(
                "MergeImage output_dir (-dir segments | seg1 seg2 ...)");
            System.err.println(
                "\toutput_dir\tname of the parent dir for output segment slice(s)");
            System.err.println(
                "\t-dir segments\tparent dir containing several segments");

            return;
        }

        Configuration conf = new Configuration();
        final FileSystem fs = FileSystem.get(conf);
        Path out = new Path(args[0]);
        ArrayList<Path> segs = new ArrayList<Path>();

        for (int i = 1; i < args.length; i++) {
            if (args[i].equals("-dir")) {
                FileStatus[] fstats = fs.listStatus(new Path(args[++i]));

                for (int j = 0; j < fstats.length; j++) {
                    segs.add(fstats[j].getPath());
                }
            } else {
                segs.add(new Path(args[i]));
            }
        }

        if (segs.size() == 0) {
            System.err.println("ERROR: No input segments.");

            return;
        }

        extract(out, segs.toArray(new Path[segs.size()]));
    }
}
