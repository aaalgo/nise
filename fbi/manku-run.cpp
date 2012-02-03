#include <boost/program_options.hpp>
#include "fbi.h"
#include "manku.h"
#include "eval.h"
#include "char_bit_cnt.inc"

using namespace std;
namespace po = boost::program_options; 
using namespace fbi;

int main (int argc, char *argv[]) {
    string db_path;
    string query_path;
    unsigned dist;
    unsigned Q;
    unsigned sample_skip;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("db", po::value(&db_path)->default_value("db"), "")
    ("query", po::value(&query_path)->default_value("query"), "")
    (",Q", po::value(&Q)->default_value(0), "0 to run through all queries")
#if 0
    ("dist,D", po::value(&dist)->default_value(1), "")
    ("skip", po::value(&skip)->default_value(4), "")
#endif
    ("sample-skip", po::value(&sample_skip)->default_value(2), "")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("db") == 0)) {
        cerr << desc;
        return 1;
    }

    Timer timer;
    timer.restart();
    MankuDB db(db_path, sample_skip);
    cerr << "Index loaded in " << timer.elapsed() << " seconds." << endl;

    for (;;) {
        string out_path;
        cout << "Please input: <dist> <out>" << std::endl;
        cin.clear();
        if (!(cin >> dist >> out_path)) break;
        ifstream is(query_path.c_str(), ios::binary);
        ofstream out(out_path.c_str());
        BOOST_VERIFY(out);

        unsigned n = 0;
        Point pt;

        Plan plan;
        vector<Key> result;
        Stat stat_result;
        Stat stat_size;
        Stat stat_time;
        unsigned size = 0;
        float time = 0;

        for (;;) {
            if ((n == Q) && (Q > 0)) break;
            /*
            if ((n % 100 == 0)) {
                system("echo 3 > /proc/sys/vm/drop_caches");
            }
            */
            if (!is.read((char *)&pt, DATA_SIZE)) break;
            timer.restart();
            db.run(pt, dist, &size, &result);
            time = timer.elapsed();
            out << result.size() << ' ' << size << ' ' << time << std::endl;
            stat_size << size;
            stat_time << time;
            stat_result << result.size();
            ++n;
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
