#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SnapshotTool
{
    struct Rgb
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
    };

    inline Rgb Mix(Rgb a, Rgb b, double t)
    {
        t = std::clamp(t, 0.0, 1.0);
        return {
            static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<std::uint8_t>(a.b + (b.b - a.b) * t)
        };
    }

    inline Rgb Shade(Rgb color, double factor)
    {
        const auto scale = [factor](int channel)
        {
            return static_cast<std::uint8_t>(std::clamp(channel * factor, 0.0, 255.0));
        };
        return { scale(color.r), scale(color.g), scale(color.b) };
    }

    struct Point
    {
        double x = 0.0;
        double y = 0.0;
    };

    class Image
    {
    public:
        Image(int width, int height, Rgb background)
            : m_width(width), m_height(height)
            , m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3)
        {
            for (std::size_t index = 0; index < m_pixels.size(); index += 3)
            {
                m_pixels[index] = background.r;
                m_pixels[index + 1] = background.g;
                m_pixels[index + 2] = background.b;
            }
        }

        int Width() const { return m_width; }
        int Height() const { return m_height; }
        const std::vector<std::uint8_t>& Pixels() const { return m_pixels; }

        // Edges are inclusive, so quads that share an edge leave no seam.
        void FillTriangle(Point a, Point b, Point c, Rgb color)
        {
            const double area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (std::abs(area) < 1e-12)
            {
                return;
            }
            const int minX = std::max(0, static_cast<int>(std::floor(std::min({ a.x, b.x, c.x }))));
            const int maxX = std::min(m_width - 1, static_cast<int>(std::ceil(std::max({ a.x, b.x, c.x }))));
            const int minY = std::max(0, static_cast<int>(std::floor(std::min({ a.y, b.y, c.y }))));
            const int maxY = std::min(m_height - 1, static_cast<int>(std::ceil(std::max({ a.y, b.y, c.y }))));
            const double sign = area > 0.0 ? 1.0 : -1.0;
            constexpr double kTolerance = -1e-9;
            for (int y = minY; y <= maxY; ++y)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    const double px = x + 0.5;
                    const double py = y + 0.5;
                    const double w0 = ((b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x)) * sign;
                    const double w1 = ((c.x - b.x) * (py - b.y) - (c.y - b.y) * (px - b.x)) * sign;
                    const double w2 = ((a.x - c.x) * (py - c.y) - (a.y - c.y) * (px - c.x)) * sign;
                    if (w0 >= kTolerance && w1 >= kTolerance && w2 >= kTolerance)
                    {
                        std::uint8_t* pixel = &m_pixels[(static_cast<std::size_t>(y) * m_width + x) * 3];
                        pixel[0] = color.r;
                        pixel[1] = color.g;
                        pixel[2] = color.b;
                    }
                }
            }
        }

        void FillQuad(const std::array<Point, 4>& q, Rgb color)
        {
            FillTriangle(q[0], q[1], q[2], color);
            FillTriangle(q[0], q[2], q[3], color);
        }

    private:
        int m_width;
        int m_height;
        std::vector<std::uint8_t> m_pixels;
    };

    // Writes the image as an (uncompressed) PNG. Returns false when the file cannot be written.
    bool WritePng(const std::string& path, const Image& image);
}
