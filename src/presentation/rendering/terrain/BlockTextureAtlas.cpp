#include "presentation/rendering/terrain/BlockTextureAtlas.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Infinitesia
{
    namespace
    {
        // Pulls a tile's rectangle in by a tiny fraction of a texel on every side.
        // The atlas uses point filtering, so a half-texel inset would squash the
        // edge texels to half size; this only guards against float rounding
        // sampling the neighbouring tile.
        Rectangle NormalisedTile(int column, int row, int tileSize, int atlasWidth, int atlasHeight)
        {
            constexpr float kInsetTexels = 0.001f;
            const float texelX = kInsetTexels / static_cast<float>(atlasWidth);
            const float texelY = kInsetTexels / static_cast<float>(atlasHeight);
            const float x0 = static_cast<float>(column * tileSize) / static_cast<float>(atlasWidth);
            const float y0 = static_cast<float>(row * tileSize) / static_cast<float>(atlasHeight);
            const float width = static_cast<float>(tileSize) / static_cast<float>(atlasWidth);
            const float height = static_cast<float>(tileSize) / static_cast<float>(atlasHeight);
            return { x0 + texelX, y0 + texelY, width - 2.0f * texelX, height - 2.0f * texelY };
        }
    }

    void BlockTextureAtlas::Load(const std::filesystem::path& directory)
    {
        if (IsLoaded())
        {
            throw std::logic_error("The block texture atlas is already loaded.");
        }
        if (!std::filesystem::is_directory(directory))
        {
            TraceLog(LOG_WARNING, "Block texture folder not found, terrain uses flat colours: %s",
                     directory.string().c_str());
            return;
        }

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".png")
            {
                files.push_back(entry.path());
            }
        }
        if (files.empty())
        {
            return;
        }
        std::sort(files.begin(), files.end());

        // Tile 0 is the white tile, the others follow in file name order.
        const std::size_t tileCount = files.size() + 1;
        const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(tileCount))));
        const int rows = static_cast<int>((tileCount + static_cast<std::size_t>(columns) - 1) /
                                          static_cast<std::size_t>(columns));

        std::vector<std::pair<std::string, Image>> images;
        int tileSize = 0;
        try
        {
            for (const auto& file : files)
            {
                const std::string path = file.string();
                Image image = LoadImage(path.c_str());
                if (image.data == nullptr)
                {
                    throw std::runtime_error("Failed to load block texture: " + path);
                }
                images.emplace_back(file.stem().string(), image);
                if (tileSize == 0)
                {
                    tileSize = image.width;
                }
                if (image.width != tileSize || image.height != tileSize)
                {
                    throw std::runtime_error(
                        "Block textures must be square and all the same size: " + path);
                }
                ImageFormat(&images.back().second, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            }

            const int atlasWidth = columns * tileSize;
            const int atlasHeight = rows * tileSize;
            Image atlas = GenImageColor(atlasWidth, atlasHeight, BLANK);
            std::unordered_map<std::string, Rectangle> tiles;
            for (std::size_t index = 0; index < images.size(); ++index)
            {
                const int slot = static_cast<int>(index) + 1;
                const int column = slot % columns;
                const int row = slot / columns;
                const Rectangle source{ 0.0f, 0.0f, static_cast<float>(tileSize), static_cast<float>(tileSize) };
                const Rectangle target{ static_cast<float>(column * tileSize), static_cast<float>(row * tileSize),
                                        static_cast<float>(tileSize), static_cast<float>(tileSize) };
                ImageDraw(&atlas, images[index].second, source, target, WHITE);
                tiles.emplace(images[index].first,
                              NormalisedTile(column, row, tileSize, atlasWidth, atlasHeight));
            }
            ImageDrawRectangle(&atlas, 0, 0, tileSize, tileSize, WHITE);

            m_texture = LoadTextureFromImage(atlas);
            UnloadImage(atlas);
            if (m_texture.id == 0)
            {
                throw std::runtime_error("Failed to upload the block texture atlas.");
            }
            SetTextureFilter(m_texture, TEXTURE_FILTER_POINT);
            SetTextureWrap(m_texture, TEXTURE_WRAP_CLAMP);
            m_white = NormalisedTile(0, 0, tileSize, atlasWidth, atlasHeight);
            m_tiles = std::move(tiles);
        }
        catch (...)
        {
            for (auto& [key, image] : images)
            {
                static_cast<void>(key);
                UnloadImage(image);
            }
            throw;
        }
        for (auto& [key, image] : images)
        {
            static_cast<void>(key);
            UnloadImage(image);
        }
    }

    void BlockTextureAtlas::Unload()
    {
        if (m_texture.id != 0)
        {
            UnloadTexture(m_texture);
        }
        m_texture = {};
        m_white = {};
        m_tiles.clear();
    }

    const Rectangle* BlockTextureAtlas::Find(std::string_view key) const
    {
        const auto tile = m_tiles.find(std::string(key));
        return tile == m_tiles.end() ? nullptr : &tile->second;
    }
}
