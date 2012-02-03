#ifndef WDONG_MANKU
#define WDONG_MANKU
namespace fbi {
    class MankuDB {
        std::vector<Index *> samples;
        std::vector<int> files;
        std::vector<Range> first;
        std::vector<Range> second;
        unsigned size;
        unsigned sample_rate;
    public:
        void computeRange (unsigned scheme, unsigned K, unsigned first, unsigned second,
                Range *r1, Range *r2) {

            unsigned first_offset = 0, first_len = 0;
            unsigned second_offset = 0, second_len = 0;

              if (scheme == 1) {
                  if (first > second) {
                      std::swap(first,second);
                  }
                  unsigned bp = DATA_BIT / K;
                  unsigned n_big = DATA_BIT % K;
                  if (first < n_big) {
                      first_offset = first * (bp + 1);
                      first_len = bp + 1;
                      if (second < n_big) {
                          second_offset = second * (bp + 1);
                          second_len = bp + 1;
                      }
                      else {
                        second_offset = n_big * (bp + 1) + (second - n_big) * bp;
                        second_len = bp;
                      }
                  }
                  else {
                      first_offset = n_big * (bp + 1) + (first - n_big) * bp;
                      first_len = bp;
                      second_offset = n_big * (bp + 1) + (second - n_big) * bp;
                      second_len = bp;
                  }
              }
              else {
                unsigned bp1 = DATA_BIT / K;
                unsigned n_big1 = DATA_BIT % K;
                if (first < n_big1) {
                    first_offset = first * (bp1 + 1);
                    first_len = bp1 + 1;
                }
                else {
                    first_offset = n_big1 * (bp1 + 1) + (first - n_big1) * bp1;
                    first_len = bp1;
                }

                unsigned l2 = DATA_BIT - first_len;
                unsigned bp2 = l2 / K;
                unsigned n_big2 = l2 % K;
                if (second < n_big2) {
                    second_offset = second * (bp2 + 1);
                    second_len = bp2 + 1;
                }
                else {
                    second_offset = n_big2 * (bp2 + 1) + (second - n_big2) * bp2;
                    second_len = bp2;
                }
                if (second_offset + second_len <= first_offset) {
                    unsigned tmp1 = second_offset;
                    unsigned tmp2 = second_len;
                    second_offset = first_offset;
                    second_len = first_len;
                    first_offset= tmp1;
                    first_len = tmp2;
                }
                else {
                    if (second_offset < first_offset) {
                        second_len += first_len;
                        first_offset = second_offset;
                        first_len = second_len;
                        second_offset = DATA_BIT;
                        second_len = 0;
                    }
                    else {
                        second_offset += first_len;
                    }
                }
              }
              r1->offset = first_offset;
              r1->length = first_len;
              r2->offset = second_offset;
              r2->length = second_len;
        }

        void copyBit (const Chunk *in, unsigned ib, unsigned ob, Chunk *out) {
            Chunk c = (in[ib / CHUNK_BIT] >> (CHUNK_BIT - 1 - (ib % CHUNK_BIT))) & 1;
            out[ob / CHUNK_BIT]
                |= c << (CHUNK_BIT - 1 - (ob % CHUNK_BIT));
        }

        void shuffle (const Chunk *in, Range r1, Range r2, Chunk *out) {
            std::fill(out, out + DATA_CHUNK, 0);
            unsigned o = 0;
            for (unsigned i = r1.offset; i < r1.offset + r1.length; ++i) {
                copyBit(in, i, o++, out);
            }
            for (unsigned i = r2.offset; i < r2.offset + r2.length; ++i) {
                copyBit(in, i, o++, out);
            }
            for (unsigned i = 0; i < r1.offset; ++i) {
                copyBit(in, i, o++, out);
            }
            for (unsigned i = r1.offset + r1.length; i < r2.offset; ++i) {
                copyBit(in, i, o++, out);
            }
            for (unsigned i = r2.offset + r2.length; i < DATA_BIT; ++i) {
                copyBit(in, i, o++, out);
            }
        }

        MankuDB (const std::string &path, unsigned sample_skip) {
            BOOST_VERIFY(sizeof(Point) == RECORD_SIZE);
            unsigned scheme;
            unsigned K;
            std::string base_dir;
            std::ifstream is(path.c_str());
            BOOST_VERIFY(is);
            is >> scheme >> K >> sample_rate;
            is >> base_dir;
            BOOST_VERIFY(is);
            if (scheme == 1) {
                size = K * (K - 1) / 2;
            }
            else {
                size = K * K;
            }
            samples.resize(size);
            files.resize(size);
            first.resize(size);
            second.resize(size);
            for (unsigned i = 0; i < size; ++i) {
                unsigned a, b;
                std::string index_path, sample_path;
                is >> a >> b >> index_path >> sample_path;
                BOOST_VERIFY(is);
                computeRange(scheme, K, a, b, &first[i], &second[i]);
                index_path = base_dir + "/" + index_path;
                sample_path = base_dir + "/" + sample_path;
                
                //std::cout << first[i].offset << ' ' << first[i].length << "  |  ";
                //std::cout << second[i].offset << ' ' << second[i].length << std::endl;
                //files[i] = new std::ifstream(index_path.c_str(), std::ios::binary);
                //BOOST_VERIFY(*files[i]);
                files[i] = open(index_path.c_str(), O_RDONLY | O_DIRECT);
                BOOST_VERIFY(files[i] >= 0);

                samples[i] = new Index(sample_path);
                BOOST_VERIFY(samples[i]);
            }
            std::cerr << "Database opened." << std::endl;
            //exit(-1);
        }

        ~MankuDB () {
            BOOST_FOREACH(Index *s, samples) {
                delete s;
            }
             BOOST_FOREACH(int f, files) {
                 close(f);
             }
        }

        void run (const Chunk *query, unsigned dist, unsigned *cost, std::vector<Key> *result) {
            Chunk qt[DATA_CHUNK];
            Selection sel;
            result->clear();
            *cost = 0;
            Scanner scanner;
            for (unsigned i = 0; i < size; ++i) {
                shuffle(query, first[i], second[i], qt);
                unsigned len = first[i].length + second[i].length;
                samples[i]->lookup(qt, len, &sel);
                *cost += sel.cost();
                scanner.setFile(files[i]);
                BOOST_FOREACH(const Range &range, sel) {
                    scanner.scan(qt, range, sample_rate, dist, result);
                }
            }
            std::sort(result->begin(), result->end());
            result->resize(std::unique(result->begin(), result->end()) - result->begin());
        }
    };
}
#endif
