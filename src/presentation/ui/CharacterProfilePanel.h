#pragma once

#include <cstddef>
#include <optional>

#include "gameplay/entities/WorldObject.h"
#include "presentation/assets/TextureAssetManager.h"
#include "raylib.h"

namespace Infinitesia
{
class CharacterProfilePanel
{
public:
	void Open(WorldObjectId character);
	void Close();
	bool IsOpen() const;
	WorldObjectId GetCharacter() const;
	bool Update(Vector2 mousePosition, bool leftClickPressed, bool escapePressed, int screenWidth, int screenHeight,
	            std::size_t slotCount);
	void Draw(const WorldObject &character, const TextureAssetManager &assets, int screenWidth, int screenHeight) const;

private:
	Rectangle GetBounds(int screenWidth, int screenHeight, std::size_t slotCount) const;
	Rectangle GetCloseButtonBounds(int screenWidth, int screenHeight, std::size_t slotCount) const;

	std::optional<WorldObjectId> m_character;
};
} // namespace Infinitesia
