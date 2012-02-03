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
    string sample_path;
    string output_path;
    unsigned first;
    unsigned sample_skip;
    unsigned sample_rate;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input,I", po::value(&sample_path), "")
    ("output,O", po::value(&output_path), "")
    ("first,F", po::value(&first)->default_value(0), "")
    ("skip", po::value(&sample_skip)->default_value(2), "")
    ("rate", po::value(&sample_rate)->default_value(1000), "")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0) || (vm.count("output") == 0)) {
        cerr << desc;
        return 1;
    }

    Trie::make(sample_path, output_path, first, sample_skip, sample_rate);

    return 0;
}
