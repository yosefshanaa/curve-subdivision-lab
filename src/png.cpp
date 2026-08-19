#include "png.h"

#include <cstdio>
#include <cstring>

namespace sl {
namespace {

// ------------------------------------------------------------- checksums

uint32_t crcTable(int i) {
    static uint32_t table[256];
    static bool built = false;
    if (!built) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        built = true;
    }
    return table[i];
}

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < len; i++) crc = crcTable((crc ^ data[i]) & 0xFF) ^ (crc >> 8);
    return crc;
}

uint32_t adler32(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

// ------------------------------------------------------------- bit writer
//
// DEFLATE fills each byte from the least significant bit upwards; Huffman
// codes are transmitted most-significant-bit first, so they get reversed.

struct BitWriter {
    std::vector<uint8_t> out;
    uint32_t bitBuf = 0;
    int bitCount = 0;

    void putBits(uint32_t value, int n) {
        bitBuf |= (value & ((1u << n) - 1u)) << bitCount;
        bitCount += n;
        while (bitCount >= 8) {
            out.push_back(uint8_t(bitBuf & 0xFF));
            bitBuf >>= 8;
            bitCount -= 8;
        }
    }
    void putCode(uint32_t code, int n) {  // MSB-first Huffman code
        uint32_t rev = 0;
        for (int i = 0; i < n; i++) rev |= ((code >> (n - 1 - i)) & 1u) << i;
        putBits(rev, n);
    }
    void flushByte() {
        if (bitCount > 0) {
            out.push_back(uint8_t(bitBuf & 0xFF));
            bitBuf = 0;
            bitCount = 0;
        }
    }
};

// ------------------------------------------------ fixed-Huffman code tables

void emitLiteral(BitWriter& bw, int lit) {
    if (lit < 144)      bw.putCode(0x30 + lit, 8);
    else if (lit < 256) bw.putCode(0x190 + lit - 144, 9);
    else if (lit < 280) bw.putCode(lit - 256, 7);
    else                bw.putCode(0xC0 + lit - 280, 8);
}

const int kLenBase[29]  = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                           31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                           2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int kDistBase[30]  = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                            193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                            6145, 8193, 12289, 16385, 24577};
const int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                            6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void emitMatch(BitWriter& bw, int len, int dist) {
    int lc = 28;
    while (lc > 0 && len < kLenBase[lc]) lc--;
    emitLiteral(bw, 257 + lc);
    if (kLenExtra[lc]) bw.putBits(uint32_t(len - kLenBase[lc]), kLenExtra[lc]);

    int dc = 29;
    while (dc > 0 && dist < kDistBase[dc]) dc--;
    bw.putCode(uint32_t(dc), 5);  // fixed distance codes are 5 bits, MSB-first
    if (kDistExtra[dc]) bw.putBits(uint32_t(dist - kDistBase[dc]), kDistExtra[dc]);
}

// --------------------------------------------------------------- LZ77

constexpr int kWindow = 32768;
constexpr int kMinMatch = 3;
constexpr int kMaxMatch = 258;
constexpr int kHashBits = 15;
constexpr int kHashSize = 1 << kHashBits;
constexpr int kMaxChain = 128;  // search depth: quality/speed knob

inline uint32_t hash3(const uint8_t* p) {
    return ((uint32_t(p[0]) << 10) ^ (uint32_t(p[1]) << 5) ^ uint32_t(p[2])) & (kHashSize - 1);
}

}  // namespace

std::vector<uint8_t> zlibCompress(const std::vector<uint8_t>& raw) {
    BitWriter bw;
    bw.out.reserve(raw.size() / 2 + 64);

    const uint8_t* d = raw.data();
    const int n = int(raw.size());

    std::vector<int> head(kHashSize, -1);
    std::vector<int> prev(raw.size() ? raw.size() : 1, -1);

    bw.putBits(1, 1);  // BFINAL — one big fixed-Huffman block
    bw.putBits(1, 2);  // BTYPE = 01 (fixed Huffman)

    int i = 0;
    while (i < n) {
        int bestLen = 0, bestDist = 0;
        if (i + kMinMatch <= n) {
            uint32_t h = hash3(d + i);
            int cand = head[h];
            int chain = 0;
            int limit = i - kWindow;
            if (limit < 0) limit = 0;
            int maxLen = n - i;
            if (maxLen > kMaxMatch) maxLen = kMaxMatch;

            while (cand >= limit && chain++ < kMaxChain) {
                if (d[cand + bestLen] == d[i + bestLen] && d[cand] == d[i]) {
                    int l = 0;
                    while (l < maxLen && d[cand + l] == d[i + l]) l++;
                    if (l > bestLen) {
                        bestLen = l;
                        bestDist = i - cand;
                        if (l >= maxLen) break;
                    }
                }
                cand = prev[cand];
            }
        }

        if (bestLen >= kMinMatch) {
            emitMatch(bw, bestLen, bestDist);
            // Insert every position covered by the match into the hash chains.
            for (int k = 0; k < bestLen; k++) {
                if (i + k + kMinMatch <= n) {
                    uint32_t h = hash3(d + i + k);
                    prev[i + k] = head[h];
                    head[h] = i + k;
                }
            }
            i += bestLen;
        } else {
            emitLiteral(bw, d[i]);
            if (i + kMinMatch <= n) {
                uint32_t h = hash3(d + i);
                prev[i] = head[h];
                head[h] = i;
            }
            i++;
        }
    }

    emitLiteral(bw, 256);  // end-of-block
    bw.flushByte();

    std::vector<uint8_t> out;
    out.reserve(bw.out.size() + 6);
    out.push_back(0x78);  // CMF: deflate, 32K window
    out.push_back(0x01);  // FLG: (0x78 << 8 | 0x01) % 31 == 0
    out.insert(out.end(), bw.out.begin(), bw.out.end());
    uint32_t ad = adler32(raw.data(), raw.size());
    out.push_back(uint8_t(ad >> 24));
    out.push_back(uint8_t(ad >> 16));
    out.push_back(uint8_t(ad >> 8));
    out.push_back(uint8_t(ad));
    return out;
}

namespace {

void be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(uint8_t(x >> 24));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x));
}

void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    be32(out, uint32_t(data.size()));
    size_t crcStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t c = crc32(out.data() + crcStart, out.size() - crcStart) ^ 0xFFFFFFFFu;
    be32(out, c);
}

inline int paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    return (pb <= pc) ? b : c;
}

}  // namespace

bool writePNG(const std::string& path, int w, int h, const std::vector<uint8_t>& rgb) {
    if (w <= 0 || h <= 0 || rgb.size() < size_t(w) * size_t(h) * 3) return false;
    const int bpp = 3;
    const size_t stride = size_t(w) * bpp;

    // Per-scanline adaptive filtering: try all five filters, keep the one with
    // the smallest sum of absolute (signed) residuals.
    std::vector<uint8_t> filtered;
    filtered.reserve((stride + 1) * size_t(h));
    std::vector<uint8_t> cand[5];
    for (int f = 0; f < 5; f++) cand[f].resize(stride);

    for (int y = 0; y < h; y++) {
        const uint8_t* cur = rgb.data() + size_t(y) * stride;
        const uint8_t* up = (y > 0) ? rgb.data() + size_t(y - 1) * stride : nullptr;

        for (size_t x = 0; x < stride; x++) {
            int a = (x >= size_t(bpp)) ? cur[x - bpp] : 0;
            int b = up ? up[x] : 0;
            int c = (up && x >= size_t(bpp)) ? up[x - bpp] : 0;
            int v = cur[x];
            cand[0][x] = uint8_t(v);
            cand[1][x] = uint8_t((v - a) & 0xFF);
            cand[2][x] = uint8_t((v - b) & 0xFF);
            cand[3][x] = uint8_t((v - ((a + b) >> 1)) & 0xFF);
            cand[4][x] = uint8_t((v - paeth(a, b, c)) & 0xFF);
        }

        int best = 0;
        long bestScore = -1;
        for (int f = 0; f < 5; f++) {
            long score = 0;
            for (size_t x = 0; x < stride; x++) {
                int s = cand[f][x];
                score += (s < 128) ? s : (256 - s);
            }
            if (bestScore < 0 || score < bestScore) {
                bestScore = score;
                best = f;
            }
        }
        filtered.push_back(uint8_t(best));
        filtered.insert(filtered.end(), cand[best].begin(), cand[best].end());
    }

    std::vector<uint8_t> out;
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    out.insert(out.end(), sig, sig + 8);

    std::vector<uint8_t> ihdr;
    be32(ihdr, uint32_t(w));
    be32(ihdr, uint32_t(h));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(2);  // colour type 2 = truecolour RGB
    ihdr.push_back(0);  // deflate
    ihdr.push_back(0);  // adaptive filtering
    ihdr.push_back(0);  // no interlace
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", zlibCompress(filtered));
    chunk(out, "IEND", {});

    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    size_t wrote = std::fwrite(out.data(), 1, out.size(), fp);
    std::fclose(fp);
    return wrote == out.size();
}

}  // namespace sl
