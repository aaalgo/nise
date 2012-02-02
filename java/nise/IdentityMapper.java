package nise;

import java.io.IOException;

import org.apache.hadoop.mapreduce.Mapper;

public class IdentityMapper<K, V> extends Mapper<K,V,K,V> {

  @Override
  public void map(K key, V value, Context context
                  ) throws IOException, InterruptedException {
    context.write(key, value);
  }
}
