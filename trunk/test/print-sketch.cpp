#include <fbi.h>
#include "../common/nise.h"

using namespace std;
using namespace nise;

int main (int argc, char *argv[]) {
    Sketch sketch;
    ImageID key;
    for (;;) {
        cin.read((char *)sketch, SKETCH_SIZE);
        key = ReadUint32(cin);
        if (!cin) break;
        fbi::PrintPoint(sketch);
    }
}
