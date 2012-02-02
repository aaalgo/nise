#include "../common/nise.h"

int main (int argc, char *argv[]) {
    for (;;) {
        std::string key;
        std::string value;
        nise::ReadStringJava(std::cin, &key);
        nise::ReadStringJava(std::cin, &value);
        if (!std::cin) break;
        std::stringstream ss(value);
        nise::Signature::RECORD.check(ss);
        nise::Record record;
        record.readFields(ss);
        std::cout << record.features.size() << std::endl;
    }
    return 0;
}
