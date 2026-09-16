#include "sha256.h"
#include <fstream>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace crypto {

namespace {

inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline uint32_t sigma0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline uint32_t sigma1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline uint32_t gamma0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline uint32_t gamma1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct SHA256Context {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
};

void sha256_init(SHA256Context* ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint32_t w[64];

    for (int i = 0; i < 16; i++) {
        w[i] = (static_cast<uint32_t>(data[i * 4]) << 24) |
               (static_cast<uint32_t>(data[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(data[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(data[i * 4 + 3]));
    }
    for (int i = 16; i < 64; i++) {
        w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
    }

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
        uint32_t t2 = sigma0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sha256_update(SHA256Context* ctx, const uint8_t* data, size_t len) {
    size_t buffer_index = static_cast<size_t>((ctx->count >> 3) & 63);
    ctx->count += static_cast<uint64_t>(len) << 3;
    size_t part_len = 64 - buffer_index;

    size_t i = 0;
    if (len >= part_len) {
        std::memcpy(&ctx->buffer[buffer_index], data, part_len);
        sha256_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 63 < len; i += 64) {
            sha256_transform(ctx->state, &data[i]);
        }
        buffer_index = 0;
    }
    std::memcpy(&ctx->buffer[buffer_index], &data[i], len - i);
}

void sha256_final(SHA256Context* ctx, uint8_t digest[32]) {
    uint8_t count_bytes[8];
    for (int i = 0; i < 8; i++) {
        count_bytes[i] = static_cast<uint8_t>((ctx->count >> ((7 - i) * 8)) & 0xff);
    }
    size_t buffer_index = static_cast<size_t>((ctx->count >> 3) & 63);
    size_t pad_len = (buffer_index < 56) ? (56 - buffer_index) : (120 - buffer_index);
    static const uint8_t padding[64] = { 0x80 };
    sha256_update(ctx, padding, pad_len);
    sha256_update(ctx, count_bytes, 8);

    for (int i = 0; i < 8; i++) {
        digest[i * 4]     = static_cast<uint8_t>((ctx->state[i] >> 24) & 0xff);
        digest[i * 4 + 1] = static_cast<uint8_t>((ctx->state[i] >> 16) & 0xff);
        digest[i * 4 + 2] = static_cast<uint8_t>((ctx->state[i] >> 8) & 0xff);
        digest[i * 4 + 3] = static_cast<uint8_t>(ctx->state[i] & 0xff);
    }
}

std::string digest_to_hex(const uint8_t digest[32]) {
    char hex[65];
    for (int i = 0; i < 32; i++) {
        snprintf(&hex[i * 2], 3, "%02x", digest[i]);
    }
    hex[64] = '\0';
    return std::string(hex);
}

} // anonymous namespace

std::string sha256_buffer(const void* data, size_t len) {
    SHA256Context ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, static_cast<const uint8_t*>(data), len);
    uint8_t digest[32];
    sha256_final(&ctx, digest);
    return digest_to_hex(digest);
}

std::string sha256_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return "";

    SHA256Context ctx;
    sha256_init(&ctx);

    char buf[65536];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        sha256_update(&ctx, reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(file.gcount()));
    }

    uint8_t digest[32];
    sha256_final(&ctx, digest);
    return digest_to_hex(digest);
}

} // namespace crypto
