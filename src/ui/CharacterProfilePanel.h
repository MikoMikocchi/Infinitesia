#pragma once

#include "raylib.h"
#include "world/World.h"

#include <cstddef>
#include <optional>

namespace Infinitesia
{
    class CharacterProfilePanel
    {
    public:
        void Open(WorldObjectId character);
        void Close();
        bool IsOpen() const;
        WorldObjectId GetCharacter() const;
        bool Update(
            Vector2 mousePosition,
            bool leftClickPressed,
            bool escapePressed,
            int screenWidth,
            int screenHeight,
            std::size_t slotCount);
        void Draw(const WorldObject& character, int screenWidth, int screenHeight) const;

    private:
        Rectangle GetBounds(int screenWidth, int screenHeight, std::size_t slotCount) const;
        Rectangle GetCloseButtonBounds(
            int screenWidth,
            int screenHeight,
            std::size_t slotCount) const;

        std::optional<WorldObjectId> m_character;
    };
}
