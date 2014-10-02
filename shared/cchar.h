#ifndef __CCHAR_H__
#define __CCHAR_H__

#include <cstring>

class cchar
{
public:
    template<typename T>
    cchar(const T &s) : str(s.data()), len(s.size()) {}
    cchar(const char *str) : str(str), len(strlen(str)) {}
    cchar(const char *str, size_t len) : str(str), len(len) {}
    cchar() : str(), len() {}

    const char *operator()() { return str; }
    template<typename T>
    bool operator==(const T &s) const { return equal(s); }
    template<typename T>
    bool operator!=(const T &s) const { return !equal(s); }
    template<typename T>

    bool equal(const T &c) const
    {
        if (len != c.size()) return false;
        return !std::memcmp(str, c.data(), len);
    }
    bool equal(const char *s) const { return !std::strcmp(str, s); }

    const char *data() const { return str; }
    const char *c_str() const { return str; }
    size_t length() const { return len; }
    size_t size() const { return len; }
private:
    const char *const str;
    const size_t len;
};

namespace std
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif // __clang__

    template<>
    struct hash<cchar>
    {
        size_t operator()(const cchar &s) const
        {
#ifdef __GLIBCXX__
            return _Hash_impl::hash(s.data(), s.length());
#elif defined(_LIBCPP_VERSION)
            return __do_string_hash(s.data(), s.data() + s.length());
#else
#error unsupported c++ library
#endif // __GLIBCXX__
        }
    };

#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
}

static inline const char *getstring(const cchar &str) { return str.c_str(); }
static inline const char *getstrtype(const cchar str) { std::abort(); }
static inline const char *getcstring(const cchar &str) { return str.c_str(); }

#endif // __CCHAR_H__
