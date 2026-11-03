#include "terrain_snapshot/Image.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace SnapshotTool
{
    namespace
    {
        // Uncompressed PNG: stored deflate blocks, so no compression library is needed.
        std::uint32_t Crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0)
        {
            static const std::array<std::uint32_t, 256> table = []
            {
                std::array<std::uint32_t, 256> t{};
                for (std::uint32_t n = 0; n < 256; ++n)
                {
                    std::uint32_t c = n;
                    for (int k = 0; k < 8; ++k)
                    {
                        c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                    }
                    t[n] = c;
                }
                return t;
            }();
            crc = ~crc;
            for (std::size_t i = 0; i < size; ++i)
            {
                crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
            }
            return ~crc;
        }

        void WriteBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value)
        {
            for (int shift = 24; shift >= 0; shift -= 8)
            {
                out.push_back(static_cast<std::uint8_t>(value >> shift));
            }
        }

        void AppendChunk(std::vector<std::uint8_t>& file, const char* type, const std::vector<std::uint8_t>& data)
        {
            WriteBigEndian(file, static_cast<std::uint32_t>(data.size()));
            std::vector<std::uint8_t> body(type, type + 4);
            body.insert(body.end(), data.begin(), data.end());
            file.insert(file.end(), body.begin(), body.end());
            WriteBigEndian(file, Crc32(body.data(), body.size()));
        }
    }

    bool WritePng(const std::string& path, const Image& image)
    {
        std::vector<std::uint8_t> raw;
        raw.reserve(static_cast<std::size_t>(image.Height()) * (static_cast<std::size_t>(image.Width()) * 3 + 1));
        for (int y = 0; y < image.Height(); ++y)
        {
            raw.push_back(0); // filter: none
            const std::uint8_t* row = &image.Pixels()[static_cast<std::size_t>(y) * image.Width() * 3];
            raw.insert(raw.end(), row, row + static_cast<std::size_t>(image.Width()) * 3);
        }

        std::vector<std::uint8_t> zlib{ 0x78, 0x01 };
        for (std::size_t offset = 0; offset < raw.size();)
        {
            const std::size_t length = std::min<std::size_t>(65535, raw.size() - offset);
            zlib.push_back(offset + length == raw.size() ? 1 : 0);
            zlib.push_back(static_cast<std::uint8_t>(length & 0xFF));
            zlib.push_back(static_cast<std::uint8_t>(length >> 8));
            zlib.push_back(static_cast<std::uint8_t>(~length & 0xFF));
            zlib.push_back(static_cast<std::uint8_t>((~length >> 8) & 0xFF));
            zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                        raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
            offset += length;
        }
        std::uint32_t a = 1;
        std::uint32_t b = 0;
        for (std::uint8_t byte : raw)
        {
            a = (a + byte) % 65521u;
            b = (b + a) % 65521u;
        }
        WriteBigEndian(zlib, (b << 16) | a);

        std::vector<std::uint8_t> file{ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        std::vector<std::uint8_t> header;
        WriteBigEndian(header, static_cast<std::uint32_t>(image.Width()));
        WriteBigEndian(header, static_cast<std::uint32_t>(image.Height()));
        header.insert(header.end(), { 8, 2, 0, 0, 0 });
        AppendChunk(file, "IHDR", header);
        AppendChunk(file, "IDAT", zlib);
        AppendChunk(file, "IEND", {});

        std::FILE* out = std::fopen(path.c_str(), "wb");
        if (out == nullptr)
        {
            return false;
        }
        const bool written = std::fwrite(file.data(), 1, file.size(), out) == file.size();
        std::fclose(out);
        return written;
    }
}
