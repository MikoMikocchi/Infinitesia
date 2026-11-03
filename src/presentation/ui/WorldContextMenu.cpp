#include "presentation/ui/WorldContextMenu.h"

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
        std::optional<WorldObjectId> hoveredTarget,
        std::optional<GridCell> hoveredCell,
        std::optional<WorldObjectId> actor,
        const std::vector<CommandOption>& targetCommands,
        const std::vector<CommandOption>& cellCommands,
        std::optional<WorldObjectId> profileTarget,
        int screenWidth,
        int screenHeight)
    {
        ContextMenuUpdate result;

        if (escapePressed && m_isOpen)
        {
            Close();
            result.consumedMouseClick = true;
            return result;
        }

        if (rightClickPressed)
        {
            if (hoveredTarget.has_value() &&
                (!targetCommands.empty() || profileTarget.has_value()))
            {
                Open(
                    mousePosition,
                    *hoveredTarget,
                    std::nullopt,
                    actor,
                    targetCommands,
                    profileTarget,
                    screenWidth,
                    screenHeight);
            }
            else if (hoveredCell.has_value() && !cellCommands.empty())
            {
                Open(
                    mousePosition,
                    std::nullopt,
                    hoveredCell,
                    actor,
                    cellCommands,
                    std::nullopt,
                    screenWidth,
                    screenHeight);
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
        for (std::size_t index = 0; index < m_commands.size(); ++index)
        {
            if (!CheckCollisionPointRec(mousePosition, GetActionBounds(index)))
            {
                continue;
            }

            const CommandOption& command = m_commands[index];
            if (command.enabled)
            {
                if (m_targetCell.has_value())
                {
                    result.cellCommandRequest = ContextMenuUpdate::CellCommandRequest{
                        command.id,
                        m_actor,
                        *m_targetCell
                    };
                }
                else if (m_target.has_value())
                {
                    result.commandRequest = CommandRequest{
                        command.id,
                        m_actor,
                        *m_target
                    };
                }
                Close();
            }
            return result;
        }

        if (m_profileTarget.has_value() &&
            CheckCollisionPointRec(mousePosition, GetActionBounds(m_commands.size())))
        {
            result.profileRequest = m_profileTarget;
            Close();
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
        DrawRectangleRec(bounds, kMenuColor);
        DrawRectangleLinesEx(bounds, 1.0f, kMenuBorderColor);

        for (std::size_t index = 0; index < m_commands.size(); ++index)
        {
            const CommandOption& command = m_commands[index];
            const Rectangle actionBounds = GetActionBounds(index);
            if (command.enabled && CheckCollisionPointRec(GetMousePosition(), actionBounds))
            {
                DrawRectangleRec(actionBounds, kHoveredItemColor);
            }

            DrawText(
                command.label.c_str(),
                static_cast<int>(m_position.x + kTextInsetX),
                static_cast<int>(m_position.y + static_cast<float>(index) * m_actionHeight + kTextInsetY),
                kFontSize,
                command.enabled ? kEnabledTextColor : kDisabledTextColor);
        }

        if (m_profileTarget.has_value())
        {
            const Rectangle profileBounds = GetActionBounds(m_commands.size());
            if (CheckCollisionPointRec(GetMousePosition(), profileBounds))
            {
                DrawRectangleRec(profileBounds, kHoveredItemColor);
            }
            DrawText(
                "Profile",
                static_cast<int>(m_position.x + kTextInsetX),
                static_cast<int>(profileBounds.y + kTextInsetY),
                kFontSize,
                kEnabledTextColor);
        }

        const char* disabledHint = GetDisabledHint();
        if (disabledHint != nullptr)
        {
            const std::size_t actionCount =
                m_commands.size() + (m_profileTarget.has_value() ? 1 : 0);
            DrawText(
                disabledHint,
                static_cast<int>(m_position.x),
                static_cast<int>(m_position.y + static_cast<float>(actionCount) * m_actionHeight + 2.0f),
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
        const float height = static_cast<float>(m_commands.size() + (m_profileTarget.has_value() ? 1 : 0)) *
                m_actionHeight +
            (HasDisabledCommands() ? m_disabledHintHeight : 0.0f);
        return { m_position.x, m_position.y, m_width, height };
    }

    Rectangle WorldContextMenu::GetActionBounds(std::size_t index) const
    {
        return {
            m_position.x,
            m_position.y + static_cast<float>(index) * m_actionHeight,
            m_width,
            m_actionHeight
        };
    }

    const char* WorldContextMenu::GetDisabledHint() const
    {
        for (const CommandOption& command : m_commands)
        {
            if (!command.enabled && !command.disabledReason.empty())
            {
                return command.disabledReason.c_str();
            }
        }
        return nullptr;
    }

    bool WorldContextMenu::HasDisabledCommands() const
    {
        for (const CommandOption& command : m_commands)
        {
            if (!command.enabled)
            {
                return true;
            }
        }
        return false;
    }

    void WorldContextMenu::Open(
        Vector2 position,
        std::optional<WorldObjectId> target,
        std::optional<GridCell> targetCell,
        std::optional<WorldObjectId> actor,
        const std::vector<CommandOption>& commands,
        std::optional<WorldObjectId> profileTarget,
        int screenWidth,
        int screenHeight)
    {
        m_isOpen = true;
        m_target = target;
        m_targetCell = targetCell;
        m_actor = actor;
        m_profileTarget = profileTarget;
        m_commands = commands;

        const float maximumX = std::max(m_screenMargin, static_cast<float>(screenWidth) - m_width - m_screenMargin);
        const float menuHeight = static_cast<float>(
                m_commands.size() + (m_profileTarget.has_value() ? 1 : 0)) *
                m_actionHeight +
            (HasDisabledCommands() ? m_disabledHintHeight : 0.0f);
        const float maximumY = std::max(m_screenMargin, static_cast<float>(screenHeight) - menuHeight - m_screenMargin);
        m_position = {
            std::clamp(position.x + m_mouseOffset, m_screenMargin, maximumX),
            std::clamp(position.y + m_mouseOffset, m_screenMargin, maximumY)
        };
    }

    void WorldContextMenu::Close()
    {
        m_isOpen = false;
        m_target.reset();
        m_targetCell.reset();
        m_actor.reset();
        m_profileTarget.reset();
        m_commands.clear();
    }
}
