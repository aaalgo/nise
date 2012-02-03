#include <sys/time.h>
#include <limits>
#include <boost/program_options.hpp>
#include "fbi.h"
#include "eval.h"
#include "char_bit_cnt.inc"

using namespace std;
namespace po = boost::program_options; 
using namespace fbi;

int main (int argc, char *argv[]) {
    string db_path;
    string query_path;
    unsigned dist;
    unsigned alg;
    unsigned task;
    unsigned Q;
    unsigned skip;
    unsigned cache;
    bool direct = false;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("db", po::value(&db_path)->default_value("db"), "")
    ("query", po::value(&query_path)->default_value("query"), "")
    (",Q", po::value(&Q)->default_value(0), "0 to run through all queries")
    ("cache", po::value(&cache)->default_value(0), "0: no cache, 1: cache ")
    ("direct", "")
#if 0
    ("dist,D", po::value(&dist)->default_value(1), "")
    ("alg", po::value(&alg)->default_value(2), "0: linear, 1: equal, 2: smart")
    ("skip", po::value(&skip)->default_value(4), "")
    ("task", po::value(&task)->default_value(1), "0: plan, 1: stat, 2: run, 3: verify")
#endif
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("db") == 0)) {
        cerr << desc;
        return 1;
    }

    if (vm.count("direct")) direct = true;

    Timer timer;
    timer.restart();
    DB db(db_path, direct);
    cerr << "Index loaded in " << timer.elapsed() << " seconds." << endl;


    for (;;) {
        string out_path;
        cout << "Please input: <dist> <algorithm> <skip> <task> <output>" << std::endl;
        cin.clear();
        if (!(cin >> dist >> alg >> skip >> task >> out_path)) break;
        ifstream is(query_path.c_str(), ios::binary);
        ofstream out(out_path.c_str());
        BOOST_VERIFY(out);

        unsigned n = 0;
        Point pt;

        Plan plan;
        vector<Key> result;
        vector<Key> ref;
        Stat stat_result;
        Stat stat_size;
        Stat stat_time;
        float size = 0;
        float time = 0;
        unsigned r_size = 0;

        DB::HitStat hit_stat;
        
        if (task == 1) {
            db.initHitStat(&hit_stat);
        }

        for (;;) {
            if ((n == Q) && (Q > 0)) break;

            /*
            if ((n % 100 == 0) && (task == 2) && (!cache)) {
                system("echo 3 > /proc/sys/vm/drop_caches");
            }
            */

            if (!is.read((char *)&pt, DATA_SIZE)) break;
            timer.restart();
            db.plan(pt, DB::Algorithm(alg), dist, skip, &plan);
            size = plan.cost();
            time = timer.elapsed();
            if (task == 1) {
                db.updateHitStat(plan, &hit_stat);
            }
            if (task >= 2) {
                db.run(pt, dist, plan, &result);
                time = timer.elapsed();
                r_size = result.size();
                stat_result << result.size();
            }
            if (task >= 3) {
                vector<Point>::const_iterator it;

                db.plan(pt, DB::LINEAR, dist, skip, &plan);
                db.run(pt, dist, plan, &ref);

                BOOST_VERIFY(result.size() == ref.size());
                for (unsigned i = 0; i < result.size(); ++i) {
                    BOOST_VERIFY(result[i] == ref[i]);
                }
                std::cerr << result.size() << " OK." << std::endl;
                time = timer.elapsed();
            }
            if (task == 0 || task == 2) {
                out << r_size << ' ' << size << ' ' << time << std::endl; // << std::flush;
            }
            stat_size << size;
            stat_time << time;
            ++n;
        }
        if (task == 1) {
            BOOST_FOREACH(const vector<unsigned> &v, hit_stat) {
                BOOST_FOREACH(unsigned vv, v) {
                    if (vv != 0) {
                        out << vv << endl;
                    }
                }
            }
        }
        out.close();
        is.close();
        cout << "[NUM] " << stat_size.getCount() << endl;
        cout << "[SIZE] " << stat_size.getAvg() << " +/- " << stat_size.getStd() << endl;
        cout << "[TIME] " << stat_time.getAvg() << " +/- " << stat_time.getStd() << endl;
        cout << "[RESULT] " << stat_result.getAvg() << " +/- " << stat_result.getStd() << endl;
    }
    return 0;
}
