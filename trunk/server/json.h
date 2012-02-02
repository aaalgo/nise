#ifndef WDONG_NISE_JSON
#define WDONG_NISE_JSON
namespace nise {

    class JSON {
        std::ostream &os;
        bool first;
        void sep () {
            if (first) {
                first = false;
            }
            else {
                os << ',';
            }
        }
    public:
        JSON (std::ostream &os_): os(os_), first(true) {
            os << '{';
        }

        ~JSON () {
            os << '}';
        }

        JSON &add (const std::string &val) {
            sep();
            os << '"' << val << '"';
            return *this;
        }

        JSON &add (const char *val) {
            sep();
            os << '"' << val << '"';
            return *this;
        }

        JSON &add (bool val) {
            sep();
            os << (val ? "true" : "false");
            return *this;
        }

        template <typename T>
        JSON &add (T val) {
            sep();
            os << val;
            return *this;
        }

        JSON &add (const std::string &key, const std::string &val) {
            sep();
            os << '"' << key << "\":\"" << val << '"';
            return *this;
        }

        JSON &add (const std::string &key, const char *val) {
            sep();
            os << '"' << key << "\":\"" << val << '"';
            return *this;
        }

        JSON &add (const std::string &key, bool val) {
            sep();
            os << '"' << key << "\":" << (val ? "true" : "false");
            return *this;
        }

        template <typename T>
        JSON &add (const std::string &key, T val) {
            sep();
            os << '"' << key << "\":" << val;
            return *this;
        }


        JSON &beginObject (const std::string &key) {
            sep();
            if (key.size()) {
                os << '"' << key << "\":";
            }
            os << '{';
            first = true;
            return *this;
        }

        JSON &endObject () {
            os << '}';
            first = false;
            return *this;
        }

        JSON &beginArray (const std::string &key) {
            sep();
            if (key.size()) {
                os << '"' << key << "\":";
            }
            os << '[';
            first = true;
            return *this;
        }

        JSON &endArray () {
            os << ']';
            first = false;
            return *this;
        }
    };
}
#endif
