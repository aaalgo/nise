package nise;

import java.io.*;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.conf.Configured;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.FileUtil;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.io.IOUtils;
import org.apache.hadoop.util.Tool;
import org.apache.hadoop.util.ToolRunner;

public class Download extends Configured implements Tool {

    public int run(String[] args) throws Exception {
        if (args.length < 2) {
            System.err.println("Download dir local");
            return 1;
        }
        OutputStream out = new FileOutputStream(args[1]);

        Path srcDir = new Path(args[0]);
        Configuration conf = new Configuration();
        FileSystem srcFS = FileSystem.get(conf);

        if (!srcFS.getFileStatus(srcDir).isDirectory()) {
            System.err.println(args[0] + " is not a directory.");
            return 1;
        }
        
        try {
          FileStatus contents[] = srcFS.listStatus(srcDir);
          for (int i = 0; i < contents.length; i++) {
            if (contents[i].isFile()) {
              System.err.println(contents[i].getPath());
              InputStream in = srcFS.open(contents[i].getPath());
              try {
                IOUtils.copyBytes(in, out, conf, false);
              } finally {
                in.close();
              } 
            }
          }
        } finally {
          out.close();
        }
        return 0;
    }

    public static void main(String[] args) throws Exception {
        int res = ToolRunner.run(new Download(), args);
        System.exit(res);
    }
}
