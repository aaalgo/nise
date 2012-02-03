#ifndef WDONG_FBI
#define WDONG_FBI

#ifdef WIN32
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <limits>
#include <boost/foreach.hpp>
#include <boost/assert.hpp>
#include <boost/multi_array.hpp>
#ifdef WIN32
typedef int omp_lock_t;
static inline void omp_init_lock(omp_lock_t *lock) {}
static inline void omp_destroy_lock(omp_lock_t *lock) {}
static inline void omp_set_lock(omp_lock_t *lock) {}
static inline void omp_unset_lock(omp_lock_t *lock) {}
static inline void *memalign(size_t boundary, size_t size) {
	return malloc(size);
}
typedef long long ssize_t;
static inline void __sync_fetch_and_add(size_t *s, size_t v) {
	*s += v;
}
#define SEP "\\"
#else
#define SEP "/"
#include <omp.h>
#endif


// index description file format (real file should not contain comments)
/*
 16                     # data bytes
 0                      # key bytes
 1000                   # sample rate
 basedir
 0 index.0    sample.0    # 32 lines
 4 index.1    sample.1
 ...
 124 index.31   sample.31
 */

namespace fbi {
////////////////////////// CONFIG SECTION /////////////////////////////

// This function guesses which device the file is on.
// This is used to ensure that only one thread is reading from a particular device.
// It would be hard to implement this in a general way, so I leave
// the user to do the work.  The easiest way is to relect the device in
// the path to the file so you can easily recognize it here.

// Even though the data & key sizes need to be specified in the index description file,
// they are actually hard coded.
// Tune the following types and constants according to your need.
    typedef unsigned char Chunk; // could be unsigned short or unsigned.
    typedef unsigned Key;        // any plain old data types
    static const unsigned DATA_BIT = 128; // can be divided by CHUNK_BIT

 ////////////////////////// END OF CONFIG SECTION /////////////////////
 
    static const unsigned CHUNK_BIT = sizeof(Chunk) * 8;
    static const unsigned DATA_CHUNK = 128 / CHUNK_BIT;
    static const unsigned DATA_SIZE = DATA_CHUNK * sizeof(Chunk);
    static const unsigned KEY_SIZE = sizeof(Key);
    static const unsigned RECORD_SIZE = DATA_SIZE + KEY_SIZE;
    static const unsigned MAX_SCAN_RESULT = 500;

    class Point {   // represents one record, should be directly readable from disk
        Chunk data[DATA_CHUNK];
        Key key;
    public:
        operator Chunk * () {
            return data;
        }
        operator const Chunk * () const {
            return data;
        }
        Key getKey () const {
            return key;
        }
    };

    static inline void Rotate (const Chunk *input, unsigned offset, Chunk *output) {
        BOOST_VERIFY(offset % 8 == 0);
        offset /= 8;
        unsigned o = 0;
        for (unsigned i = offset; i < DATA_CHUNK; ++i) {
            output[o++] = input[i];
        }
        for (unsigned i = 0; i < offset; ++i) {
            output[o++] = input[i];
        }
    }

    static inline void Unrotate (const Chunk *input, unsigned offset, Chunk *output) {
        BOOST_VERIFY(offset % 8 == 0);
        offset /= 8;
        unsigned o = 0;
        for (unsigned i = offset; i < DATA_CHUNK; ++i) {
            output[i] = input[o++];
        }
        for (unsigned i = 0; i < offset; ++i) {
            output[i] = input[o++];
        }
    }


    class Hamming
    {
        static unsigned char_bit_cnt[];
        template <typename B>
        unsigned __hamming (B a, B b)
        {
            B c = a ^ b;
            unsigned char *p = reinterpret_cast<unsigned char *>(&c);
            unsigned r = 0;
            for (unsigned i = 0; i < sizeof(B); i++)
            {
                r += char_bit_cnt[*p++];
            }
            return r;
        }
    public:
        float operator () (const Chunk *first1, const Chunk *first2) 
        {
            unsigned r = 0;
            for (unsigned i = 0; i < DATA_CHUNK; ++i)
            {
                r += __hamming(first1[i], first2[i]);
            }
            return float(r);
        }
    };

    struct Range {      // scan range
        unsigned offset;
        unsigned length;
    };

    static inline void PrintPoint (const Chunk *data) {
        for (unsigned i = 0; i < 128 / 8; ++i) {
            printf("%02X", data[i]);
        }
        printf("\n");
    }

    // a sliding window over a bit vector
    class Window {
        unsigned size;
        unsigned cur;
        int left;
    public:
        Window (unsigned size_, unsigned first_ = 0, int left_ = DATA_BIT)
            : size(size_), cur(first_), left(left_) {
        }

        Chunk peek (const Chunk *data) const {
            unsigned idx = cur / CHUNK_BIT;
            unsigned idx_next = (idx + 1) % DATA_CHUNK;
            unsigned left = cur % CHUNK_BIT;
            unsigned right = CHUNK_BIT - left;
            if (right >= size) {
                return (data[idx] >>  (right - size)) & ((1 << size) - 1);
            }
            else {
                return ((data[idx] << (size - right)) + (data[idx_next] >> (CHUNK_BIT + right - size))) & ((1 << size) - 1);
            }
        }

        Window next () const {
            return Window(size, (cur + size) % DATA_BIT, left - size);
        }

        void incr () {
            cur = (cur + size) % DATA_BIT;
            left -= size;
        }

        unsigned skip () const {
            return size;
        }

        operator bool () const {    // test if the window is in NON-ZERO position (cur != first)
            return left > 0;
        }
    };


    // the union of a set of contiguous regions
    // representing the candidate set of a dataset to be scanned
    class Selection: public std::vector<Range> {
    public:
        size_t cost () const {
            size_t v = 0;
            BOOST_FOREACH(const Range &r, *this) {
                v += r.length;
            }
            return v;
        }
    };

    class Index {
    public:
        struct Trie {
            Range range;
            int children;
        };
    private:
        Trie trie;
        unsigned first;
        unsigned sample_skip;
        unsigned size;
        std::vector<Trie> entries;
    public:

        Index (const std::string &sample_path) {
            unsigned len;
            std::ifstream is(sample_path.c_str(), std::ios::binary);
            is.read((char *)&first, sizeof(first));
            is.read((char *)&sample_skip, sizeof(sample_skip));
            is.read((char *)&len, sizeof(len));
            entries.resize(len);
            is.read((char *)&entries[0], len * sizeof(Trie));
            size = 1 << sample_skip;
        }

        Selection all () {
            Selection sel;
            sel.push_back(entries[0].range);
            return sel;
        }

        unsigned max () {
            return entries[0].range.length;
        }

#if 0
        void explore () {
            std::string c;
            unsigned cur = 0;
            while (std::cin >> c) {
                if (c == "p") {
                    std::cout << cur << ": [" << entries[cur].range.offset << ": " << entries[cur].range.length << ": " << entries[cur].range.offset + entries[cur].range.length << "]" << std::endl;
                    unsigned next = entries[cur].children;
                    if (next){
                        for (unsigned i = 0; i < size; ++i) {
                            std::cout << i << ": " << next + i << ": ";
                            std::cout << '[' << entries[next + i].range.offset << ": " << entries[next + i].range.length << ": " << entries[next + i].range.offset + entries[next + i].range.length << "]" << std::endl;
                        }
                    }
                }
                else if (c == "s") {
                    std::cin >> cur;
                }
                else if (c == "c") {
                    unsigned child;
                    std::cin >> child;
                    if (entries[cur].children != 0) {
                        cur = entries[cur].children + child;
                    }
                }
            }
        }
#endif

        void lookup (const Chunk *query, unsigned key, Selection *ext) {
            Window window(sample_skip, first);
            unsigned cur = 0, next;
            unsigned n_sskip = key / sample_skip;
            unsigned left = key % sample_skip;
            ext->clear();
            for (;;) {
                if (n_sskip == 0) break;
                //next = cur->lookup(window.peek(query));
                next = entries[cur].children;
                if (next == 0) break;
                cur = next + window.peek(query);
                --n_sskip;
                window.incr();
            }
            unsigned off = entries[cur].children;
            if ((n_sskip > 0) || (left == 0) || (off == 0)) {
                ext->push_back(entries[cur].range);
            }
            else {
                // one level left over
                unsigned tail = sample_skip - left;
                BOOST_VERIFY(sizeof(unsigned) > sizeof(Chunk));

                unsigned beg = window.peek(query) >> tail;
                unsigned end = ((beg + 1) << tail) - 1;
                beg = beg << tail;

                Range br = entries[off + beg].range;//cur->lookup(beg)->getRange();
                Range er = entries[off + end].range;//cur->lookup(end)->getRange();

                br.length = er.offset + er.length - br.offset;

                ext->push_back(br);
            }
        }

        void lookup (const Chunk *query, unsigned skip, unsigned len, std::vector<Selection> *ext)
        {
            BOOST_VERIFY(skip % sample_skip == 0);
            unsigned pos = 0;
            Window window(sample_skip, first);
            ext->clear();
            //const Trie *cur = &trie, *next;
            unsigned cur = 0, next;
            while (ext->size() < len) {
                //std::cout << ' ' << entries[cur].range.length;
                next = entries[cur].children;
                if (next == 0) break;
                next += window.peek(query);
                cur = next;
                window.incr();
                pos += sample_skip;
                if (pos % skip == 0) {
                    ext->push_back(Selection());
                    ext->back().push_back(entries[cur].range);
                }
            }
            while (ext->size() < len) {
                ext->push_back(Selection());
                ext->back().push_back(entries[cur].range);
            }
        }
    };


    class Plan: public std::vector<Selection>
    {
    public:
        Plan (): std::vector<Selection>(DATA_BIT) {
            reset();
        }

        void reset () {
            BOOST_FOREACH(Selection &s, *this) {
                s.clear();
            }
        }

        size_t cost () const {
            size_t v = 0;
            BOOST_FOREACH(const Selection &s, *this) {
                v += s.cost();
            }
            return v;
        }
    };


    struct Access {
        unsigned file;
        Range range;
        unsigned query;
    };

    static inline bool operator < (const Access &ac1, const Access &ac2) {
        if (ac1.file < ac2.file) return true;
        if (ac1.file > ac2.file) return false;
        if (ac1.range.offset < ac2.range.offset) return true;
        if (ac1.range.offset + ac1.range.length < ac2.range.offset + ac2.range.length) return true;
        return false;
    }

    class Scanner {
         int file;
         size_t buffer_size;
         size_t block_size;

         char *region;
     public:
         Scanner (size_t buffer_size_ = 10 * 1024 * 1024, size_t block_size_ = 512) : file(-1),
            buffer_size(buffer_size_),
            block_size(block_size_)
         {
            BOOST_VERIFY(buffer_size >= block_size * 3);
            BOOST_VERIFY(block_size >= RECORD_SIZE);
            region = (char *)memalign(block_size, buffer_size);
            BOOST_VERIFY(region);
         }

         ~Scanner () {
             free(region);
         }

         void setFile (int f) {
             file = f;
         }

         void scan (const Chunk *query, Range range, unsigned sample_rate, unsigned dist, std::vector<Key> *result, omp_lock_t *lock = 0)
         {
            BOOST_VERIFY(file >= 0);

            Hamming hamming;

            int64_t off = int64_t(range.offset) * RECORD_SIZE * sample_rate; // offset of the first point
            int64_t pos = off / block_size * block_size; // beginning reading position


            size_t picked = 0;
            size_t cnt = size_t(range.length) * sample_rate; // # points to scan

            size_t total_size = size_t((off + cnt * RECORD_SIZE + block_size - 1) / block_size * block_size
                                    - pos); // total size to be read

            size_t batch_size = (buffer_size / block_size - 2) * block_size;

            size_t skip_size = size_t(off - pos);
            size_t read_offset = 0;

#ifdef WIN32
            pos -= _lseeki64(file, pos, SEEK_SET);
#else
            pos -= lseek(file, pos, SEEK_SET);
#endif
            BOOST_VERIFY(pos == 0);

            while ((total_size > 0) && (cnt > 0)) {
                if (total_size < batch_size) {
                    batch_size = total_size;
                }
                BOOST_VERIFY(read_offset + batch_size <= buffer_size);

                char *begin = region;
#ifdef WIN32
                ssize_t s = _read(file, begin + read_offset, batch_size);
#else
                ssize_t s = read(file, begin + read_offset, batch_size);
#endif
                
                if (s < 0) {
                    std::cerr << strerror(errno) << std::endl;
                }
                //BOOST_VERIFY(s > 0);
                if (s <= 0) break;
                total_size -= size_t(s);

                size_t left_over = (read_offset + s - skip_size) % sizeof(Point);

                Point *pt = (Point *)(begin + skip_size);
                Point *end = (Point *)(begin + read_offset + s - left_over);

                while ((cnt > 0) && (pt < end)) {
                    if (hamming(query, *pt) < dist) {
                        if (lock) {
                            omp_set_lock(lock);
                            result->push_back(pt->getKey());
                            omp_unset_lock(lock);
                        }
                        else {
                            result->push_back(pt->getKey());
                        }
                        ++picked;
                        if (picked >= MAX_SCAN_RESULT) {
                            return;
                        }
                    }
                    ++pt;
                    --cnt;
                }


                read_offset = block_size;
                skip_size = block_size - (left_over % block_size);

                std::memmove(region + skip_size, (void *)end, left_over);
            }
            /*
            BOOST_VERIFY(cnt == 0);
            BOOST_VERIFY(total_size == 0);
            */
         }
     };

    class DB {
        //unsigned key_size;
        unsigned db_size;
        unsigned sample_rate;
        std::vector<Index *> samples;
        std::vector<int> files;
        std::vector<unsigned> disk;
        std::vector<size_t> stat;
    protected:
        unsigned getSampleRate () const {
            return sample_rate;
        }

        const std::vector<int> &getFiles () const {
            return files;
        }

    private:

        //std::vector<int> files;
        //
        void planLinear (const Chunk *query, unsigned dist, Plan *pl) const
        {
            pl->reset();
            for (unsigned i = 0; i < DATA_BIT; ++i) {
                // find a non empty file
                if ((samples[i] != 0) && (files[i] != 0)) {
                    pl->at(i) = samples[i]->all();
                    return;
                }
            }
            BOOST_VERIFY(0);
        }

        void planAll (const Chunk *query, unsigned dist, Plan *pl) const
        {
	    std::vector<unsigned> good;
	    for (unsigned i = 0; i < DATA_BIT; ++i) {
		    if (samples[i]) good.push_back(i);
	    }

            pl->reset();;
            for (unsigned i = 0; i < good.size(); i++) {
		unsigned b_p;
		if (i + 1 < good.size()) {
			b_p = good[i+1] - good[i];
		}
		else {
			b_p = DATA_BIT - good[i] + good[0];
		}
		std::cout << good[i] << ":" << b_p << std::endl;
                samples[good[i]]->lookup(query, b_p, &pl->at(good[i]));
            }
        }

        void planEqual (const Chunk *query, unsigned dist, Plan *pl) const
        {
            unsigned n_p = dist; // # partition
            unsigned b_p = DATA_BIT / dist + 1; // bit / partition
            unsigned n_big = DATA_BIT % dist; // # partitions with one extra bit
            unsigned cur = 0;

            pl->reset();;
            for (unsigned i = 0; i < n_p; i++) {
                if (i == n_big) {
                    --b_p;
                }
                BOOST_VERIFY(samples[cur]);
                samples[cur]->lookup(query, b_p, &pl->at(cur));
                cur += b_p;
            }
        }

        struct SubPlan {
            size_t cost;
            unsigned next;
        };

        void planSmart (const Chunk *query, unsigned dist, unsigned skip, Plan *pl) const
        {
            unsigned n_p = dist; // # partition
            unsigned size = DATA_BIT / skip;
            BOOST_VERIFY(DATA_BIT % skip == 0);

            SubPlan ZERO_SUBPLAN = {0, 0};

            typedef boost::multi_array<SubPlan, 2> WorkSheet;
            WorkSheet A(boost::extents[n_p][size]);

            std::vector<std::vector<Selection> > lookup(size);
            std::vector<bool> good(size);
            // good[i] : i * step is a valid partitioning point

            std::fill(good.begin(), good.end(), false);

            for (unsigned i = 0; i < size; i++) {
                if (samples[i * skip] != NULL) {
                    good[i] = true;
                    samples[i * skip]->lookup(query, skip, size, &lookup[i]);
                }
                /*
                BOOST_FOREACH(const Selection &s, lookup[i]) {
                    std::cout << ' ' << s.cost();
                }
                */
            }

            size_t best =  std::numeric_limits<size_t>::max();

            for (unsigned start = 0; start <= size - n_p; ++start) {

                if (!good[start]) continue;

                std::fill_n(A.data(), A.num_elements(), ZERO_SUBPLAN);
                            // fill with bad partitioning

                unsigned add = (n_p-1);;
                unsigned sub = 1;

                // c = 0, 0 split
                for (unsigned n = start + add; n <= size - sub; ++n) {
                    if (!good[n]) continue;
                    A[0][n].cost = lookup[n][size-n+start-1].cost();
                }

                for (unsigned c = 1; c < n_p; ++c) {
                    ++sub;
                    --add;
                    for (unsigned n = start + add; n <= size - sub; ++n) {
                        if (!good[n]) continue;
                        size_t cost = std::numeric_limits<size_t>::max();
                        unsigned next = 0;
                        for (unsigned m = n + 1; m <= size-sub+1; ++m) {
                            if (!good[m]) continue;
                            if (A[c-1][m].cost == std::numeric_limits<size_t>::max()) continue;
                            size_t s = lookup[n][m-n-1].cost()
                                        + A[c-1][m].cost;
                            if (s < cost) {
                                cost = s;
                                next = m;
                            }
                        }
                        // BOOST_VERIFY(cost < std::numeric_limits<size_t>::max());
                        A[c][n].cost = cost;
                        A[c][n].next = next;
                    }
                }
                BOOST_VERIFY(add == 0);
                if (A[n_p-1][start].cost < best) {
                    best = A[n_p-1][start].cost;
                    pl->reset();
                    unsigned k = start;
                    unsigned l = n_p - 1;
                    for (;;) {
                        unsigned next = A[l][k].next;
                        if (l == 0) {
                            BOOST_VERIFY(next == 0);
                            pl->at(k * skip) = lookup[k][size - k + start - 1];
                            break;
                        }
                        else {
                            pl->at(k * skip) = lookup[k][next - k - 1];
                            k = next;
                            --l;
                        }
                    }
                }
            }
        }

    public:
        DB (const std::string &path, bool direct = false) {
            BOOST_VERIFY(sizeof(Point) == RECORD_SIZE);
            db_size = 0;
            std::string base_dir;
            std::ifstream is(path.c_str());
            BOOST_VERIFY(is);
            unsigned data_byte, key_byte;
            is >> data_byte >> key_byte >> sample_rate;
            is >> base_dir;
            BOOST_VERIFY(is);
            BOOST_VERIFY(data_byte == DATA_SIZE);
            BOOST_VERIFY(key_byte == KEY_SIZE);
            samples.resize(DATA_BIT);
            files.resize(DATA_BIT);
            disk.resize(DATA_BIT);
            stat.resize(DATA_BIT);
            fill(samples.begin(), samples.end(), (Index*)0);
            fill(files.begin(), files.end(), -1);
            fill(stat.begin(), stat.end(), 0);
            for (;;) {
                unsigned idx, disk_id;
                std::string index_path, sample_path;
                if (!(is >> idx >> disk_id >> index_path >> sample_path)) {
                    break;
                }
                BOOST_VERIFY(is);
                index_path = base_dir + SEP + index_path;
                sample_path = base_dir + SEP + sample_path;
                // std::cerr << "Loading index " << index_path << "..." << std::endl;
                
                disk[idx] = disk_id;
#ifdef WIN32
                files[idx] = _open(index_path.c_str(), _O_RDONLY | _O_BINARY);
#else
                files[idx] = open(index_path.c_str(), O_RDONLY | (direct ? O_DIRECT : 0));
#endif
                BOOST_VERIFY(files[idx] >= 0);
                //files[idx] = new std::ifstream(index_path.c_str(), std::ios::binary);
                //BOOST_VERIFY(*files[idx]);

                samples[idx] = new Index(sample_path);
                if (db_size == 0) {
                    db_size = samples[idx]->max();
                }
                else {
                    BOOST_VERIFY(db_size == samples[idx]->max());
                }
                BOOST_VERIFY(samples[idx]);
            }

            std::cerr << "Database opened." << std::endl;
        }

        ~DB () {
            BOOST_FOREACH(Index *s, samples) {
                if (s) {
                    delete s;
                }
            }
            BOOST_FOREACH(int f, files) {
                if (f >= 0) {
#ifdef WIN32
		    _close(f);
#else
                    close(f);
#endif
                }
            }
        }

        const std::vector<size_t> &getStat () const {
            return stat;
        }

        enum Algorithm {
            LINEAR, ALL, EQUAL, SMART
        };

        void plan (const Chunk *query, Algorithm alg, unsigned dist, unsigned skip, Plan *pl) const {
            if (alg == LINEAR) {
                planLinear(query, dist, pl);
            }
            else if (alg == EQUAL) {
                planEqual(query, dist, pl);
            }
            else if (alg == ALL) {
                planAll(query, dist, pl);
            }
            else if (alg == SMART) {
                planSmart(query, dist, skip, pl);
            }
            else BOOST_VERIFY(0);
        }

        unsigned cost (const Plan &plan) {
            return plan.cost() * sample_rate;
        }

        typedef std::vector<std::vector<unsigned> > HitStat;

        void initHitStat (HitStat *stat) {
            stat->resize(samples.size());
            for (unsigned i = 0; i < samples.size(); ++i) {
                if (samples[i]) {
                    stat->at(i).resize(samples[i]->max());
                    std::fill(stat->at(i).begin(), stat->at(i).end(), 0);
                }
            }
        }

        void updateHitStat (const Plan &plan, HitStat *stat) {
            for (unsigned i = 0; i < plan.size(); ++i) {
                if (files[i] == 0) continue;
                std::vector<unsigned> &st = stat->at(i);
                BOOST_FOREACH(const Range &range, plan[i]) {
                    for (unsigned j = 0; j < range.length; ++j) {
                        ++st[range.offset + j];
                    }
                }
            }
        }

        void run (const Chunk *query, unsigned dist, const Plan &plan, std::vector<Key> *result) {
            result->clear();
            Scanner scanner;
            for (unsigned i = 0; i < plan.size(); ++i) {
                if (plan[i].empty()) continue;
                __sync_fetch_and_add(&stat[i], 1);
                scanner.setFile(files[i]);
                BOOST_FOREACH(const Range &range, plan[i]) {
                    scanner.scan(query, range, sample_rate, dist, result);
                }
            }
            std::sort(result->begin(), result->end());
            result->resize(std::unique(result->begin(), result->end()) - result->begin());
        }

    private:

        typedef std::vector<Access> AccessList;

    public:

        void batch (const std::vector<Chunk *> &queries,
                Algorithm alg, unsigned plan_dist, unsigned dist, unsigned skip,
                std::vector<std::vector<Key> > *results) {

            std::vector<Plan> plans(queries.size());
            // plan
#pragma omp parallel for default(shared)
            for (int i = 0; i < int(queries.size()); ++i) {
                plan(queries[i], alg, plan_dist, skip, &plans[i]);
            }
            // sort
            static const unsigned NUM_DISK = 4;
            std::vector<AccessList> all(NUM_DISK);

            for (unsigned i = 0; i < queries.size(); ++i) {
                for (unsigned j = 0; j < plans[i].size(); ++j) {
                    if (plans[i][j].empty()) continue;
                    ++stat[j];
                    __sync_fetch_and_add(&stat[j], 1);
                    AccessList &al = all[disk[j]];
                    BOOST_FOREACH(const Range &range, plans[i][j]) {
                        Access ac;
                        ac.file = j;
                        ac.range = range;
                        ac.query = i;
                        al.push_back(ac);
                    }
                }
            }

            results->clear();
            results->resize(queries.size());
 
            std::vector<omp_lock_t> locks(queries.size());
            for (unsigned i = 0; i < queries.size(); ++i) {
                omp_init_lock(&locks[i]);
            }
#pragma omp parallel for default(shared)
            for (int i = 0; i < int(NUM_DISK); ++i) {
                std::sort(all[i].begin(), all[i].end());
                Scanner scanner;
                BOOST_FOREACH(Access ac, all[i]) {
                    scanner.setFile(files[ac.file]);
                    scanner.scan(queries[ac.query], ac.range, sample_rate, dist, &results->at(ac.query), &locks[ac.query]);
                }
            }
            for (unsigned i = 0; i < queries.size(); ++i) {
                omp_destroy_lock(&locks[i]);
            }

#pragma omp parallel for default(shared)
            for (int i = 0; i < int(queries.size()); ++i) {
                std::sort(results->at(i).begin(), results->at(i).end());
                results->at(i).resize(std::unique(results->at(i).begin(), results->at(i).end()) - results->at(i).begin());
            }
        }
    };

}

namespace fbi {

    static const Range ZERO_RANGE = {0, 0};

    static inline int Compare (const Chunk *c1, const Chunk *c2, unsigned first) {
        unsigned first_chunk = first / CHUNK_BIT;
        unsigned first_shift = first % CHUNK_BIT;
        Chunk a, b;

        a = c1[first_chunk] << first_shift;
        b = c2[first_chunk] << first_shift;

        if (a < b) return -1;
        if (a > b) return 1;

        for (unsigned i = first_chunk + 1; i < DATA_CHUNK; ++i) {
            if (c1[i] < c2[i]) return -1;
            if (c1[i] > c2[i]) return 1;
        }
        
        for (unsigned i = 0; i < first_chunk; ++i) {
            if (c1[i] < c2[i]) return -1;
            if (c1[i] > c2[i]) return 1;
        }

        a = c1[first_chunk] >> (CHUNK_BIT - first_shift);
        b = c2[first_chunk] >> (CHUNK_BIT - first_shift);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }

    class Trie {
        // valid after update()
        Range range;

        int childIndex;

        union {
            // internal node, childIndex != 0
            struct {
                Trie *children;
                unsigned size;
            };
            // leaf node, childIndex == 0
            struct {
                const Chunk *data;
                unsigned addr;
            };
        };

        void updateBegin (unsigned *v) {
            range.offset = *v;
            if (childIndex) {
                for (unsigned i = 0; i < size; ++i) {
                    children[i].updateBegin(v);
                }
            }
            else if (data) {
                BOOST_VERIFY(addr >= *v);
                *v = addr;
            }
        }

        void updateEnd (unsigned *v) {
            BOOST_VERIFY(*v >= range.offset);
            range.length = *v - range.offset;
            if (childIndex) {
                for (unsigned i = size; i > 0; --i) {
                    children[i-1].updateEnd(v);
                }
            }
            else if (data) {
                BOOST_VERIFY(addr < *v);
                *v = addr;
            }
        }

        void updateChildIndex (int *idx) {
            if (childIndex) {
                childIndex = *idx;
                *idx += size;
                for (unsigned i = 0; i < size; ++i) {
                    children[i].updateChildIndex(idx);
                }
            }
        }
        
        void saveHelper (std::ofstream &os) const {
            if (childIndex) {
                std::vector<Index::Trie> entries(size);
                for (unsigned i = 0; i < size; ++i) {
                    entries[i].range = children[i].range;
                    entries[i].children = children[i].childIndex;
                }
                os.write((const char *)&entries[0], sizeof(entries[0]) * size);
                for (unsigned i = 0; i < size; ++i) {
                    children[i].saveHelper(os);
                }
            }
        }

    public:
        Trie (): range(ZERO_RANGE), childIndex(0), children(0), size(0) {
        }

        ~Trie () {
            if (childIndex) {
                delete[] children;
            }
        }

        void insert (const Chunk *data_, size_t addr_, const Window &window) {
            if (childIndex == 0) {
                if (data == 0) {
                    data = data_;
                    addr = addr_;
                    return;
                }
                if (!window) { // bits used up
                    // BOOST_VERIFY(Compare(data, data_, 0) == 0);
                    return;
                }
                // split
                const Chunk *data_copy = data;
                unsigned addr_copy = addr;

                childIndex = -1;
                size = 1 << window.skip();
                children = new Trie[size];
                Chunk K = window.peek(data);
                BOOST_VERIFY(K < size);
                children[K].data = data_copy;
                children[K].addr = addr_copy;
            }
            BOOST_VERIFY(window);
            Chunk K = window.peek(data_);
            BOOST_VERIFY(K < size);
            children[K].insert(data_, addr_, window.next());
        }

        static void make (const std::string &path, const std::string &output, unsigned first, unsigned sample_skip, unsigned sample_rate, bool nokey = false) {

            Trie trie;

            unsigned addr = 0;
            {
                unsigned record_size = nokey ? DATA_SIZE : RECORD_SIZE;
                unsigned skip_size = record_size  * (sample_rate - 1);
                Window window(sample_skip, first);
                std::ifstream is(path.c_str(), std::ios::binary);
                BOOST_VERIFY(sizeof(Point) == RECORD_SIZE);
                Point pt;
                while(is.read((char *)&pt, record_size)) {
                    trie.insert(pt, addr, window);
                    addr += 1;
                    is.seekg(skip_size, std::ios::cur);
                }
                //std::cout << addr << std::endl;
            }
            unsigned begin = 0;
            unsigned end = addr;
            trie.updateBegin(&begin);
            trie.updateEnd(&end);
            int childIndex = 1;
            trie.updateChildIndex(&childIndex);

            std::ofstream os(output.c_str(), std::ios::binary);
            os.write((const char *)&first, sizeof(first));
            os.write((const char *)&sample_skip, sizeof(sample_skip));
            os.write((const char *)&childIndex, sizeof(childIndex));
            Index::Trie entry;
            entry.range = trie.range;
            entry.children = 1;
            os.write((const char *)&entry, sizeof(entry));
            trie.saveHelper(os);
        }
    };
}

#endif
