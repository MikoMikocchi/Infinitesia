#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "raylib.h"

namespace Infinitesia {
// Packs every PNG of a folder (one square tile each, all the same size) into a
// single texture when the game starts. A tile is found by its file name without
// the extension: "rock.granite.png" -> "rock.granite", "block.grass_top.png" ->
// "block.grass_top". Adding a texture means dropping a PNG into the folder -
// nothing is listed in code.
class BlockTextureAtlas {
 public:
  BlockTextureAtlas() = default;
  BlockTextureAtlas(const BlockTextureAtlas&) = delete;
  BlockTextureAtlas& operator=(const BlockTextureAtlas&) = delete;

  // Needs an OpenGL context. A missing folder leaves the atlas empty (the
  // terrain then falls back to flat colours); a tile of a different size or an
  // unreadable PNG throws.
  void Load(const std::filesystem::path& directory);
  void Unload();

  bool IsLoaded() const { return m_texture.id != 0; }
  const Texture2D& GetTexture() const { return m_texture; }

  // The tile's rectangle in normalised texture coordinates, pulled in by half a
  // texel so that filtering never reads a neighbouring tile; nullptr when there
  // is no such tile.
  const Rectangle* Find(std::string_view key) const;

  // A fully white tile, for faces that are drawn with a flat vertex colour.
  Rectangle WhiteTile() const { return m_white; }

 private:
  Texture2D m_texture{};
  Rectangle m_white{};
  std::unordered_map<std::string, Rectangle> m_tiles;
};
}  // namespace Infinitesia
