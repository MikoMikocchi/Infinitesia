#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "raylib.h"

namespace Infinitesia
{
class TextureAssetManager
{
public:
	TextureAssetManager() = default;
	TextureAssetManager(const TextureAssetManager &) = delete;
	TextureAssetManager &operator=(const TextureAssetManager &) = delete;

	void LoadTextures(const std::filesystem::path &assetDirectory);
	void UnloadTextures();
	const Texture2D &GetTexture(std::string_view assetId) const;

private:
	std::unordered_map<std::string, Texture2D> m_textures;
	bool m_loaded = false;
};
} // namespace Infinitesia
