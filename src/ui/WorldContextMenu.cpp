#include "ui/WorldContextMenu.h"

#include <algorithm>

namespace Infinitesia
{
    namespace
    {
        constexpr float kTextInsetX = 12.0f;
        constexpr float kTextInsetY = 14.0f;
        constexpr int kFontSize = 20;
        constexpr Color kMenuColor = { 34, 39, 45, 245 };
        constexpr Color kMenuBorderColor = { 100, 110, 120, 255 };
        constexpr Color kEnabledTextColor = { 240, 240, 240, 255 };
        constexpr Color kDisabledTextColor = { 125, 130, 135, 255 };
        constexpr Color kHoveredItemColor = { 65, 75, 85, 255 };
        constexpr Color kDisabledHintColor = { 190, 170, 115, 255 };
    }

    ContextMenuUpdate WorldContextMenu::Update(
        Vector2 mousePosition,
        bool rightClickPressed,
        bool leftClickPressed,
        bool escapePressed,
        std::optional<WorldObjectId> hoveredTree,
        std::optional<WorldObjectId> selectedWorker,
        int screenWidth,
        int screenHeight)
    {
        ContextMenuUpdate result;
        m_actionEnabled = selectedWorker.has_value();

        if (escapePressed && m_isOpen)
        {
            Close();
            result.consumedMouseClick = true;
            return result;
        }

        if (rightClickPressed)
        {
            if (hoveredTree.has_value())
            {
                Open(mousePosition, *hoveredTree, selectedWorker, screenWidth, screenHeight);
            }
            else
            {
                Close();
            }
            result.consumedMouseClick = true;
            return result;
        }

        if (!m_isOpen || !leftClickPressed)
        {
            return result;
        }

        result.consumedMouseClick = true;
        if (CheckCollisionPointRec(mousePosition, GetActionBounds()))
        {
            if (m_actionEnabled && m_actor.has_value())
            {
                result.actionRequest = WorldActionRequest{ WorldAction::Chop, *m_actor, m_target };
                Close();
            }
            return result;
        }

        Close();
        return result;
    }

    void WorldContextMenu::Draw() const
    {
        if (!m_isOpen)
        {
            return;
        }

        const Rectangle bounds = GetBounds();
        const bool hovered = m_actionEnabled &&
            CheckCollisionPointRec(GetMousePosition(), GetActionBounds());
        DrawRectangleRec(bounds, kMenuColor);
        DrawRectangleLinesEx(bounds, 1.0f, kMenuBorderColor);
        if (hovered)
        {
            DrawRectangleRec(GetActionBounds(), kHoveredItemColor);
        }

        DrawText(
            "Chop",
            static_cast<int>(m_position.x + kTextInsetX),
            static_cast<int>(m_position.y + kTextInsetY),
            kFontSize,
            m_actionEnabled ? kEnabledTextColor : kDisabledTextColor);

        if (!m_actionEnabled)
        {
            DrawText(
                "Select a worker first",
                static_cast<int>(m_position.x),
                static_cast<int>(m_position.y + m_actionHeight + 2.0f),
                kFontSize - 4,
                kDisabledHintColor);
        }
    }

    bool WorldContextMenu::BlocksWorldInput(Vector2 mousePosition) const
    {
        return m_isOpen && CheckCollisionPointRec(mousePosition, GetBounds());
    }

    Rectangle WorldContextMenu::GetBounds() const
    {
        const float height = m_actionHeight + (m_actionEnabled ? 0.0f : m_disabledHintHeight);
        return { m_position.x, m_position.y, m_width, height };
    }

    Rectangle WorldContextMenu::GetActionBounds() const
    {
        return { m_position.x, m_position.y, m_width, m_actionHeight };
    }

    void WorldContextMenu::Open(
        Vector2 position,
        WorldObjectId target,
        std::optional<WorldObjectId> actor,
        int screenWidth,
        int screenHeight)
    {
        m_isOpen = true;
        m_target = target;
        m_actor = actor;

        const float maximumX = std::max(m_screenMargin, static_cast<float>(screenWidth) - m_width - m_screenMargin);
        const float menuHeight = m_actionHeight + (actor.has_value() ? 0.0f : m_disabledHintHeight);
        const float maximumY = std::max(m_screenMargin, static_cast<float>(screenHeight) - menuHeight - m_screenMargin);
        m_position = {
            std::clamp(position.x + m_mouseOffset, m_screenMargin, maximumX),
            std::clamp(position.y + m_mouseOffset, m_screenMargin, maximumY)
        };
    }

    void WorldContextMenu::Close()
    {
        m_isOpen = false;
        m_actor.reset();
    }
}
