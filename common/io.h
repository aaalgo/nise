#ifndef WDONG_NISE_IO
#define WDONG_NISE_IO

/* This file defines data structures
 * and protocols used for communication
 * between various components of the search
 * engine.
 */
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/assert.hpp>
#include <Poco/ByteOrder.h>

namespace nise {

    // I/O routines
    static inline uint16_t ReadUint16 (std::istream &is) {
        uint16_t v;
        is.read((char *)&v, sizeof(v));
        return v;
    }

    static inline uint32_t ReadUint32 (std::istream &is) {
        uint32_t v;
        is.read((char *)&v, sizeof(v));
        return v;
    }

    static inline void WriteUint32 (std::ostream &os, uint32_t v) {
        os.write((char *)&v, sizeof(v));
    }

    static inline void WriteUint64 (std::ostream &os, uint64_t v) {
        os.write((char *)&v, sizeof(v));
    }

    static inline void ReadString (std::istream &is, std::string *str) {
        size_t sz = ReadUint32(is);
        BOOST_VERIFY(sz <= MAX_BINARY);
        str->resize(sz);
        if (sz) {
            is.read(&str->at(0), sz);
        }
    }

    static inline void WriteString (std::ostream &os, const std::string &str) {
        BOOST_VERIFY(str.size() <= MAX_BINARY);
        WriteUint32(os, str.size());
        if (str.size()) {
            os.write(&str[0], str.size());
        }
    }

    template <typename T>
    void ReadVector (std::istream &is, std::vector<T> *v) {
        size_t sz = ReadUint32(is);
        BOOST_VERIFY(sz <= MAX_BINARY);
        v->resize(sz);
        if (sz) {
            is.read(reinterpret_cast<char *>(&v->at(0)), sz * sizeof(v->at(0)));
        }
    }

    template <typename T>
    void WriteVector (std::ostream &os, const std::vector<T> &v) {
        WriteUint32(os, v.size());
        if (v.size()) {
            os.write(reinterpret_cast<const char *>(&v[0]), v.size() * sizeof(v[0]));
        }
    }

    static inline void WriteStringJava (std::ostream &os, const std::string &str) {
        BOOST_VERIFY(str.size() <= MAX_BINARY);
        WriteUint32(os, Poco::ByteOrder::toBigEndian(uint32_t(str.size())));
        if (str.size()) {
            os.write(&str[0], str.size());
        }
    }

    static inline void ReadStringJava (std::istream &is, std::string *str) {
        uint32_t sizeBig, size;
        sizeBig = ReadUint32(is);
        size = Poco::ByteOrder::fromBigEndian(sizeBig);
        BOOST_VERIFY(size < MAX_BINARY);
        str->resize(size);
        if (size) {
            is.read(&str->at(0), size);
        }
    }

    class Binary;

    static inline void ReadFile (const std::string &path, std::string *binary) {
		// file could not be too big
        binary->clear();
        std::ifstream is(path.c_str(), std::ios::binary);
        if (!is) return;
        is.seekg(0, std::ios::end);
        size_t size = size_t(is.tellg());
        if (size > MAX_BINARY) return;
        binary->resize(size);
        if (size) {
            is.seekg(0, std::ios::beg);
            is.read((char *)&binary->at(0), size);
        }
        if (!is) binary->clear();
    }

    static inline uint32_t ParseUint32 (const std::string &s) {
        std::stringstream ss(s);
        uint32_t v = ReadUint32(ss);
        BOOST_VERIFY(ss);
        return v;
    }

    static inline std::string EncodeUint32 (uint32_t v) {
        std::ostringstream ss(std::ios::binary);
        WriteUint32(ss, v);
        return ss.str();
    }

    static inline uint16_t ParseUint16Java (const std::string &s) {
        std::stringstream ss(s);
        uint16_t v = ReadUint16(ss);
        BOOST_VERIFY(ss);
        return Poco::ByteOrder::fromBigEndian(v);
    }

    static inline uint32_t ParseUint32Java (const std::string &s) {
        std::stringstream ss(s);
        uint32_t v = ReadUint32(ss);
        BOOST_VERIFY(ss);
        return Poco::ByteOrder::fromBigEndian(v);
    }

    static inline std::string EncodeUint32Java (uint32_t v) {
        std::ostringstream ss(std::ios::binary);
        WriteUint32(ss, Poco::ByteOrder::toBigEndian(v));
        return ss.str();
    }

    static inline std::string EncodeUint64 (uint64_t v) {
        std::ostringstream ss(std::ios::binary);
        WriteUint64(ss, v);
        return ss.str();
    }

    std::string StringToHex (const std::string &);
    std::string HexToString (const std::string &);
}


#endif
