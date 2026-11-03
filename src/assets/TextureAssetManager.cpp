#include "assets/TextureAssetManager.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        bool IsTextureFile(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });

            return extension == ".bmp" || extension == ".gif" ||
                extension == ".hdr" || extension == ".jpg" ||
                extension == ".jpeg" || extension == ".pic" ||
                extension == ".png" || extension == ".pnm" ||
                extension == ".qoi" || extension == ".psd" ||
                extension == ".tga";
        }

        void ReleaseTextures(std::unordered_map<std::string, Texture2D>& textures)
        {
            for (const auto& [assetId, texture] : textures)
            {
                static_cast<void>(assetId);
                UnloadTexture(texture);
            }
            textures.clear();
        }
    }

    void TextureAssetManager::LoadTextures(const std::filesystem::path& assetDirectory)
    {
        if (m_loaded)
        {
            throw std::logic_error("Texture assets are already loaded.");
        }
        if (!std::filesystem::is_directory(assetDirectory))
        {
            throw std::runtime_error(
                "Texture asset directory does not exist: " + assetDirectory.string());
        }

        std::unordered_map<std::string, Texture2D> loadedTextures;
        try
        {
            for (std::filesystem::recursive_directory_iterator iterator(assetDirectory), end;
                 iterator != end;
                 ++iterator)
            {
                // Block tiles are packed into one atlas by BlockTextureAtlas, not loaded one by one.
                if (iterator->is_directory() &&
                    std::filesystem::relative(iterator->path(), assetDirectory).generic_string() ==
                        "textures/blocks")
                {
                    iterator.disable_recursion_pending();
                    continue;
                }
                if (!iterator->is_regular_file() || !IsTextureFile(iterator->path()))
                {
                    continue;
                }

                const std::string assetId =
                    std::filesystem::relative(iterator->path(), assetDirectory)
                        .replace_extension()
                        .generic_string();
                const std::string texturePath = iterator->path().string();
                const Texture2D texture = LoadTexture(texturePath.c_str());
                if (texture.id == 0)
                {
                    throw std::runtime_error("Failed to load texture: " + texturePath);
                }

                const auto [entry, inserted] = loadedTextures.emplace(assetId, texture);
                if (!inserted)
                {
                    UnloadTexture(texture);
                    throw std::runtime_error("Duplicate texture asset ID: " + assetId);
                }
                static_cast<void>(entry);
            }
        }
        catch (...)
        {
            ReleaseTextures(loadedTextures);
            throw;
        }

        m_textures = std::move(loadedTextures);
        m_loaded = true;
    }

    void TextureAssetManager::UnloadTextures()
    {
        ReleaseTextures(m_textures);
        m_loaded = false;
    }

    const Texture2D& TextureAssetManager::GetTexture(std::string_view assetId) const
    {
        const auto texture = m_textures.find(std::string(assetId));
        if (texture == m_textures.end())
        {
            throw std::out_of_range("Texture asset was not loaded: " + std::string(assetId));
        }
        return texture->second;
    }
}
