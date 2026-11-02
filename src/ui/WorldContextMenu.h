#pragma once

#include "raylib.h"
#include "world/World.h"

#include <optional>

namespace Infinitesia
{
    enum class WorldAction
    {
        Chop
    };

    struct WorldActionRequest
    {
        WorldAction action;
        WorldObjectId actor;
        WorldObjectId target;
    };

    struct ContextMenuUpdate
    {
        bool consumedMouseClick = false;
        std::optional<WorldActionRequest> actionRequest;
    };

    class WorldContextMenu
    {
    public:
        ContextMenuUpdate Update(
            Vector2 mousePosition,
            bool rightClickPressed,
            bool leftClickPressed,
            bool escapePressed,
            std::optional<WorldObjectId> hoveredTree,
            std::optional<WorldObjectId> selectedWorker,
            int screenWidth,
            int screenHeight);

        void Draw() const;
        bool BlocksWorldInput(Vector2 mousePosition) const;

    private:
        static constexpr float m_width = 184.0f;
        static constexpr float m_actionHeight = 48.0f;
        static constexpr float m_disabledHintHeight = 22.0f;
        static constexpr float m_screenMargin = 8.0f;
        static constexpr float m_mouseOffset = 6.0f;

        Rectangle GetBounds() const;
        Rectangle GetActionBounds() const;
        void Open(Vector2 position, WorldObjectId target, std::optional<WorldObjectId> actor, int screenWidth, int screenHeight);
        void Close();

        bool m_isOpen = false;
        Vector2 m_position{ 0.0f, 0.0f };
        WorldObjectId m_target{ 0 };
        std::optional<WorldObjectId> m_actor;
        bool m_actionEnabled = false;
    };
}
