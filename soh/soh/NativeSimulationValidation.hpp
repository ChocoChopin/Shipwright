#pragma once
// Test-only, streaming semantic validation. Never used by gameplay.
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace NativeValidation {
using nlohmann::json;
// Portable SHA-256, fed directly by the typed semantic tree (no JSON text).
class Sha256 {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint8_t buffer[64]{};
    uint64_t count = 0;
    static uint32_t R(uint32_t x, unsigned n) { return (x >> n) | (x << (32-n)); }
    void Block() {
        static constexpr uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64];
        for (unsigned i=0;i<16;++i) w[i]=(uint32_t(buffer[i*4])<<24)|(uint32_t(buffer[i*4+1])<<16)|
            (uint32_t(buffer[i*4+2])<<8)|buffer[i*4+3];
        for (unsigned i=16;i<64;++i) w[i]=w[i-16]+(R(w[i-15],7)^R(w[i-15],18)^(w[i-15]>>3))+
            w[i-7]+(R(w[i-2],17)^R(w[i-2],19)^(w[i-2]>>10));
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for (unsigned i=0;i<64;++i) {
            uint32_t t=v+(R(e,6)^R(e,11)^R(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            uint32_t u=(R(a,2)^R(a,13)^R(a,22))+((a&b)^(a&c)^(b&c));
            v=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
public:
    void Byte(uint8_t b) { buffer[count++%64]=b; if (!(count%64)) Block(); }
    void U64(uint64_t x) { for (int i=7;i>=0;--i) Byte(uint8_t(x>>(i*8))); }
    void Text(const std::string& s) { U64(s.size()); for (uint8_t c:s) Byte(c); }
    std::string Finish() {
        const auto bits=count*8; Byte(128); while (count%64!=56) Byte(0); U64(bits);
        std::string result; const char* hex="0123456789abcdef";
        for (auto x:h) for (int i=7;i>=0;--i) result+=hex[(x>>(i*4))&15];
        return result;
    }
};
// Format semantic-sha256-v1: tags n/b/i/f/s/a/o, big-endian u64 lengths,
// signed-magnitude integers, IEEE binary64 floats, sorted UTF-8 object keys.
// Signed/unsigned storage of nonnegative JSON integers is deliberately identical.
inline void Feed(Sha256& h, const json& v) {
    if (v.is_null()) h.Byte('n');
    else if (v.is_boolean()) { h.Byte('b');h.Byte(v.get<bool>()); }
    else if (v.is_number_integer()) {
        h.Byte('i'); const bool negative=!v.is_number_unsigned() && v.get<int64_t>()<0;
        h.Byte(negative);h.U64(negative ? uint64_t(-(v.get<int64_t>()+1))+1 : v.get<uint64_t>());
    } else if (v.is_number_float()) {
        double x=v.get<double>(); if (!std::isfinite(x)) throw std::runtime_error("non-finite semantic number");
        uint64_t bits;std::memcpy(&bits,&x,8);h.Byte('f');h.U64(bits);
    } else if (v.is_string()) { h.Byte('s');h.Text(v.get_ref<const std::string&>()); }
    else if (v.is_array()) { h.Byte('a');h.U64(v.size());for (const auto& x:v) Feed(h,x); }
    else if (v.is_object()) {
        h.Byte('o');h.U64(v.size());for (auto it=v.begin();it!=v.end();++it) { h.Text(it.key());Feed(h,it.value()); }
    } else throw std::runtime_error("unsupported semantic type");
}
inline std::string Hash(const json& v) { Sha256 h;Feed(h,v);return h.Finish(); }

template<class T, size_t Capacity> class Ring {
    std::array<T,Capacity> records{};
    size_t next=0, used=0;
public:
    void Push(T record) { records[next]=std::move(record);next=(next+1)%Capacity;if (used<Capacity) ++used; }
    template<class F> void Visit(F f) const { for (size_t i=0;i<used;++i) f(records[(next+Capacity-used+i)%Capacity]); }
    size_t Size() const { return used; }
};

inline const json& Field(const json& v, const std::string& path) {
    const json* p=&v;size_t start=0;
    do { auto end=path.find('.',start);auto key=path.substr(start,end-start);
        p=p->is_array()? &p->at(std::stoul(key)) : &p->at(key);
        if (end==std::string::npos) return *p;start=end+1;
    } while (true);
}
// One accumulator per declared assertion; no retained snapshot series.
class Assertions {
    struct Check { json spec, first, last, observed;std::set<std::string> distinct;
        double low=0, high=0, distance=0;uint64_t hits=0, falling=0, landing=0;bool inputSeen=false,inputOkay=false; };
    std::vector<Check> checks;
    uint64_t count=0;
public:
    explicit Assertions(const json& fixture) {
        for (const auto& a:fixture.value("assertions",json::array())) {
            const std::string kind=a.at("kind");
            const std::set<std::string> kinds={"changes","distance","range","decreases","at_least","counter_advance",
                "ever_bits","ever_nonzero","input","fall_landing"};
            if (!kinds.count(kind)) throw std::runtime_error("unknown fixture assertion kind");
            checks.push_back(Check{a});
        }
    }
    void Add(const json& row) {
        if (row.at("tick")!=count || row.at("time_q")!=count*6 || row.at("schema")!=1 || row.size()<=3)
            throw std::runtime_error("invalid semantic boundary");
        if (row.contains("player") && row.at("player").value("action","")=="unmapped")
            throw std::runtime_error("unmapped Player action");
        std::set<std::string> identities;
        for (const auto& a:row.value("actors",json::array())) {
            auto id=a.at("identity").get<std::string>();
            if (id=="untracked" || !identities.insert(id).second) throw std::runtime_error("invalid actor identity");
        }
        for (auto& c:checks) {
            std::string kind=c.spec.at("kind");
            if (kind=="input") {
                if (row.at("tick")==c.spec.at("tick")) {
                    c.inputSeen=true;c.inputOkay=true;c.observed=json::object();
                    for (auto it=c.spec.begin();it!=c.spec.end();++it) if (it.key()!="kind" && it.key()!="tick") {
                        c.observed[it.key()]=row.at("input").at(it.key());
                        c.inputOkay &= Hash(it.value())==Hash(c.observed[it.key()]);
                    }
                } continue;
            }
            const json& value=kind=="fall_landing" ? Field(row,"player.position.y.value") : Field(row,c.spec.at("field"));
            if (!count) {
                c.first=value;
                if (value.is_number()) c.low=c.high=value.get<double>();
            }
            c.last=value;
            if (kind=="changes") {
                // Threshold proof needs no unbounded set of successful values.
                const auto required=std::max(0,c.spec.value("minimum_distinct",2));
                if (c.distinct.size()<static_cast<size_t>(required)) c.distinct.insert(Hash(value));
            }
            else if (kind=="distance") {
                double sum=0;for (auto axis:{"x","y","z"}) { double d=value.at(axis).at("value").get<double>()-c.first.at(axis).at("value").get<double>();sum+=d*d; }
                c.distance=std::max(c.distance,std::sqrt(sum));
            } else if (kind=="ever_bits") {
                auto mask=c.spec.at("mask").get<int64_t>();if (count && (value.get<int64_t>()&mask)==mask) ++c.hits;
            } else if (kind=="ever_nonzero") { if (count && value!=0) ++c.hits; }
            else if (kind=="fall_landing") {
                if (count && Field(row,"player.velocity.y.value").get<double>()<=-1 && !(Field(row,"player.bg_flags").get<unsigned>()&1) && !c.falling) c.falling=count;
                if (c.falling && count>c.falling && (Field(row,"player.bg_flags").get<unsigned>()&1) &&
                    c.first.get<double>()-value.get<double>()>=c.spec.at("minimum_drop").get<double>()) ++c.landing;
                c.low=std::min(c.low,value.get<double>());
            } else { c.low=std::min(c.low,value.get<double>());c.high=std::max(c.high,value.get<double>()); }
        }
        ++count;
    }
    json Result() const {
        json rows=json::array();bool all=true;
        for (const auto& c:checks) {
            std::string k=c.spec.at("kind");bool pass=false;json observed;
            if (k=="changes") { observed=c.distinct.size();pass=static_cast<int64_t>(c.distinct.size())>=c.spec.value("minimum_distinct",2); }
            else if (k=="distance") { observed=c.distance;pass=c.distance>=c.spec.at("minimum").get<double>(); }
            else if (k=="range") { observed=c.high-c.low;pass=observed.get<double>()>=c.spec.at("minimum_span").get<double>(); }
            else if (k=="decreases") { observed=c.first.get<double>()-c.low;pass=observed.get<double>()>=c.spec.at("minimum_drop").get<double>(); }
            else if (k=="at_least") { observed=c.high;pass=c.high>=c.spec.at("minimum").get<double>(); }
            else if (k=="counter_advance") { observed=c.last.get<int64_t>()-c.first.get<int64_t>();pass=observed==c.spec.at("amount"); }
            else if (k=="ever_bits" || k=="ever_nonzero") { observed={{"matching_boundaries",c.hits}};pass=c.hits>0; }
            else if (k=="input") { observed=c.observed;pass=c.inputSeen&&c.inputOkay; }
            else if (k=="fall_landing") { observed={{"first_fall",c.falling},{"landing_count",c.landing},{"maximum_drop",c.first.get<double>()-c.low}};pass=c.falling&&c.landing; }
            all &= pass;rows.push_back({{"assertion",c.spec},{"status",pass?"pass":"fail"},{"observed",observed}});
            if (k=="changes") rows.back()["distinct_count_saturated"]=pass;
        }
        return {{"schema",1},{"status",all?"pass":"fail"},{"boundaries",count},{"assertions",rows}};
    }
};
} // namespace NativeValidation
