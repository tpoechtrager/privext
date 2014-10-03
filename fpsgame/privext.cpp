#include <GeoIP.h>
#include <enet/enet.h>
#include <stdlib.h>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cchar.h>

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

namespace privext {
namespace {

struct range;
struct pool;

constexpr size_t maxpoolsize = 100u;
constexpr const char *whoisdbname = "GeoIPCountryWhois.csv";
constexpr const char *geoipdbname = "GeoIP.dat";
std::unordered_map<cchar, pool*> ipmap;
bool randomips = !!getenv("PRIVEXT_ENABLE_IP_RANDOMIZATION");
GeoIP *geoip;

void seedrng()
{
    unsigned int sv = 0u;

#if __has_builtin(__builtin_readcyclecounter)
    sv = (unsigned int)__builtin_readcyclecounter();
#else
    sv = (unsigned int)time(NULL);
#endif

    sv += enet_time_get();
    srand(sv);
}

unsigned int getrandomnum(unsigned int max) { return rand() % max; }

struct range
{
    uint32_t start;
    uint32_t end;

    bool valid() const { return end >= start; }
    size_t numips() const { return end - start; }
};

struct pool
{
    std::vector<range> ranges;

    size_t numranges() const { return ranges.size(); }

    bool isfull() const
    {
        const size_t n = numranges();
        return (randomips ? n >= maxpoolsize : n >= 1u);
    }

    bool addrange(const range &r)
    {
        if (isfull()) return false;
        ranges.push_back(r);
        return true;
    }

    uint32_t getip(bool verify = false) const
    {
        if (!randomips) return ranges[0].start;
        seedrng();
        const range &r = ranges[getrandomnum(numranges())];
        uint32_t ip = getrandomnum(r.end - r.start + 1) + r.start;
        if (verify && (ip < r.start || ip > r.end)) abort();
        return ip;
    }
};

inline bool skip(const char *&str, int c)
{
    while (*str && *str != c) ++str;
    if (*str) ++str;
    return *str;
}

inline const char *findstaticstr(const char *cc)
{
    if (!cc[0] || !cc[1] || cc[2]) return "unknown";
    for (auto &scc : GeoIP_country_code)
        if (*(uint16_t*)scc == *(uint16_t*)cc) return scc;
    return "unknown";
}

inline pool *getpool(const char *cc, bool add = false)
{
    auto p = ipmap.find(cc);
    if (p != ipmap.end()) return p->second;
    if (!add) return nullptr;
    pool *np = new pool;
    ipmap.insert(std::pair<cchar, pool*>(findstaticstr(cc), np));
    return np;
}

void initipmap()
{
    std::ifstream whoisdb(whoisdbname);
    std::string line;
    size_t numranges = 0;
    size_t numaddedranges = 0;
    char cc[4];
    range r;

    if (!whoisdb.good())
    {
        std::cerr << "cannot open " << whoisdbname << std::endl;
        return;
    }
    
    memset(cc, '\0', sizeof(cc));

    while (std::getline(whoisdb, line))
    {
        // "1.0.0.0","1.0.0.255","16777216","16777471","AU","Australia"

        const char *str = line.c_str(); // parse as C string

        if (!skip(str, ',')) continue; // ipstr 1
        if (!skip(str, ',')) continue; // ipstr 2
        if (!skip(str, '"')) continue; // ipnum 1

        r.start = strtoul(str, NULL, 10);

        if (!skip(str, '"')) continue;
        if (!skip(str, ',')) continue;
        if (!skip(str, '"')) continue; // ipnum 2

        r.end = strtoul(str, NULL, 10);

        if (!r.valid()) continue;

        if (!skip(str, '"')) continue;
        if (!skip(str, ',')) continue;
        if (!skip(str, '"')) continue; // country code

        const char *pcc = str;

        if (!skip(str, '"')) continue;
        if (str - pcc != 3) continue;

        memcpy(cc, pcc, 2);

        ++numranges;
        if (r.numips() <= 4096u) continue;
        if (getpool(cc, true)->addrange(r)) ++numaddedranges;
    }

    std::cerr << "privext:  countries: " << ipmap.size()
              << "  ranges: " << numranges
              << "  added ranges: " << numaddedranges << std::endl;
}

void deinitmap()
{
    for (auto &c : ipmap) delete c.second;
    ipmap.clear();
}

void savememory() { for (auto &c : ipmap) c.second->ranges.shrink_to_fit(); }

void verify()
{
    if (!geoip) return;

    uint32_t ip;
    const char *code;
    size_t numips = 0u;
    size_t failcnt = 0u;

    for (auto &c : ipmap)
    {
        std::cerr << "verifying " << c.first.c_str() << std::endl;
        c.second->getip(true);

        for (const range &r : c.second->ranges)
        {
            ip = r.start;
            numips += r.numips();

            for (ip = r.start; ip <= r.end; ++ip)
            {
                code = GeoIP_country_code_by_ipnum(geoip, ip);
                if (!code) code = "unknown";

                if (c.first != code)
                {
                    ++failcnt;

                    std::cerr << "privext: country does not match: "
                              << c.first.c_str()
                              << " != " << code << std::endl;
                }
            }
        }
    }

    std::cerr << "checked " << numips << " ip addresses, "
              << "fail count: " << failcnt << std::endl;
}

void initgeoip() { geoip = GeoIP_open(geoipdbname, GEOIP_MEMORY_CACHE); }

void deinitgeoip()
{
    if (!geoip) return;
    GeoIP_delete(geoip);
}

struct init
{
    init()
    {
        initgeoip();
        initipmap();
        savememory();
        if (getenv("PRIVEXT_VERIFY_IPS")) verify();
    }
    ~init()
    {
        deinitmap();
        deinitgeoip();
    }
} init;

} // anonymous namespace

uint32_t getip(const char *countrycode)
{
    if (!countrycode) return 0u;
    pool *p = getpool(countrycode);
    if (!p) return 0u;
    return ENET_HOST_TO_NET_32(p->getip());
}

uint32_t getip(uint32_t ip)
{
    if (!geoip) return 0u;
    ip = ENET_HOST_TO_NET_32(ip);
    return getip(GeoIP_country_code_by_ipnum(geoip, ip));
}

} // namespace privext
