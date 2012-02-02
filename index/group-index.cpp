#include "../common/nise.h"

using namespace std;

int main (int argc, char *argv[]) {

    for (;;) {
        uint64_t off = cin.tellg();
        nise::Signature::CONTAINER.check(cin);
        if (!cin) break;
        uint32_t id = nise::ReadUint32(cin);
        uint32_t size = nise::ReadUint32(cin);
        uint32_t cnt = nise::ReadUint32(cin);
        cerr << id << '\t' << off << '\t' << size << '\t' << cnt << endl;
        BOOST_VERIFY(cnt <= nise::CONTAINER_SIZE);
        nise::WriteUint64(cout, off);
        for (unsigned i = 0; i < cnt; ++i) {
            nise::Record record;
            nise::ImageID iid = nise::ReadUint32(cin);
            record.readFields(cin);
        }
    }

}
