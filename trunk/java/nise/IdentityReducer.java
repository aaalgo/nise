package nise;

import java.io.IOException;

import org.apache.hadoop.mapreduce.Reducer;

/** A {@link Reducer} that passes all keys and values to output. */
public class IdentityReducer<K, V> extends Reducer<K,V,K,V> {

  public void reduce(K key, Iterable<V> values, 
                     Context context) throws IOException, InterruptedException {
    for (V value : values) {
      context.write(key, value);
    }
  }

}
