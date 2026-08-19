// png.h — self-contained PNG writer.
//
// The project links against nothing but libc/libdl, so the DEFLATE compressor
// (LZ77 + fixed Huffman) and the CRC-32/Adler-32 checksums are implemented
// here rather than pulled in from zlib.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sl {

// Compress `raw` with DEFLATE (fixed-Huffman blocks) and wrap it in a zlib stream.
std::vector<uint8_t> zlibCompress(const std::vector<uint8_t>& raw);

// Write an 8-bit RGB image (`rgb` is w*h*3 bytes, row-major, top row first).
bool writePNG(const std::string& path, int w, int h, const std::vector<uint8_t>& rgb);

}  // namespace sl
