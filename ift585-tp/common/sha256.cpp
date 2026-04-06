// =============================================================
//  sha256.cpp  –  Implémentation SHA-256 portable
//  IFT585 – TP4   (RFC 6234 / FIPS 180-4)
// =============================================================
#include "sha256.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>

// ---- Constantes SHA-256 ----
static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z)  { return (x & y) ^ (~x & z); }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t S0(uint32_t x) { return rotr(x,2)  ^ rotr(x,13) ^ rotr(x,22); }
static inline uint32_t S1(uint32_t x) { return rotr(x,6)  ^ rotr(x,11) ^ rotr(x,25); }
static inline uint32_t s0(uint32_t x) { return rotr(x,7)  ^ rotr(x,18) ^ (x >> 3);  }
static inline uint32_t s1(uint32_t x) { return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

struct SHA256Context {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buf[64];
    uint32_t buflen;

    SHA256Context() : bitlen(0), buflen(0) {
        state[0] = 0x6a09e667; state[1] = 0xbb67ae85;
        state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
        state[4] = 0x510e527f; state[5] = 0x9b05688c;
        state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
    }

    void transform(const uint8_t* block) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i]  = (uint32_t)block[i*4+0] << 24;
            w[i] |= (uint32_t)block[i*4+1] << 16;
            w[i] |= (uint32_t)block[i*4+2] <<  8;
            w[i] |= (uint32_t)block[i*4+3];
        }
        for (int i = 16; i < 64; i++)
            w[i] = s1(w[i-2]) + w[i-7] + s0(w[i-15]) + w[i-16];

        uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
        uint32_t e=state[4],f=state[5],g=state[6],h=state[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + S1(e) + ch(e,f,g) + K[i] + w[i];
            uint32_t t2 = S0(a) + maj(a,b,c);
            h=g; g=f; f=e; e=d+t1;
            d=c; c=b; b=a; a=t1+t2;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            buf[buflen++] = data[i];
            if (buflen == 64) { transform(buf); bitlen += 512; buflen = 0; }
        }
    }

    std::string finalize() {
        bitlen += buflen * 8;
        buf[buflen++] = 0x80;
        if (buflen > 56) {
            while (buflen < 64) buf[buflen++] = 0;
            transform(buf); buflen = 0;
        }
        while (buflen < 56) buf[buflen++] = 0;
        for (int i = 7; i >= 0; i--) { buf[buflen++] = (bitlen >> (i*8)) & 0xff; }
        transform(buf);

        std::ostringstream oss;
        for (int i = 0; i < 8; i++)
            oss << std::hex << std::setw(8) << std::setfill('0') << state[i];
        return oss.str();
    }
};

namespace SHA256 {

std::string hash(const unsigned char* data, size_t len) {
    SHA256Context ctx;
    ctx.update(data, len);
    return ctx.finalize();
}

std::string hash(const std::string& data) {
    return hash(reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
}

std::string hashFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    SHA256Context ctx;
    char buf[4096];
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0)
        ctx.update(reinterpret_cast<const uint8_t*>(buf), (size_t)f.gcount());
    return ctx.finalize();
}

} // namespace SHA256
