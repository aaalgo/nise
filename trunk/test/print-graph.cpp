#include "../common/nise.h"

using namespace std;
using namespace nise;

int main (int argc, char *argv[]) {
    ImageID key;
    std::vector<ImageID> value;
    for (;;) {
        key = ReadUint32(cin);
        ReadVector<ImageID>(cin, &value);
        if (!cin) break;
        cout << key;
        BOOST_FOREACH(ImageID v, value) {
            cout << ' ' << v; 
        }
        cout << endl;
    }
}
