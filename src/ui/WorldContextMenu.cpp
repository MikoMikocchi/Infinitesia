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
        std::optional<WorldObjectId> hoveredTarget,
        std::optional<WorldObjectId> selectedWorker,
        const std::vector<WorkerCommandOption>& targetCommands,
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
            if (hoveredTarget.has_value() && !targetCommands.empty())
            {
                Open(
                    mousePosition,
                    *hoveredTarget,
                    selectedWorker,
                    targetCommands,
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

            const WorkerCommandOption& command = m_commands[index];
            if (command.enabled && m_actor.has_value())
            {
                result.commandRequest = WorkerCommandRequest{
                    command.id,
                    *m_actor,
                    m_target
                };
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
        DrawRectangleRec(bounds, kMenuColor);
        DrawRectangleLinesEx(bounds, 1.0f, kMenuBorderColor);

        for (std::size_t index = 0; index < m_commands.size(); ++index)
        {
            const WorkerCommandOption& command = m_commands[index];
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

        const char* disabledHint = GetDisabledHint();
        if (disabledHint != nullptr)
        {
            DrawText(
                disabledHint,
                static_cast<int>(m_position.x),
                static_cast<int>(m_position.y + static_cast<float>(m_commands.size()) * m_actionHeight + 2.0f),
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
        const float height = static_cast<float>(m_commands.size()) * m_actionHeight +
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
        if (!m_actor.has_value())
        {
            return "Select a worker first";
        }

        for (const WorkerCommandOption& command : m_commands)
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
        if (!m_actor.has_value())
        {
            return true;
        }

        for (const WorkerCommandOption& command : m_commands)
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
        WorldObjectId target,
        std::optional<WorldObjectId> actor,
        const std::vector<WorkerCommandOption>& commands,
        int screenWidth,
        int screenHeight)
    {
        m_isOpen = true;
        m_target = target;
        m_actor = actor;
        m_commands = commands;

        const float maximumX = std::max(m_screenMargin, static_cast<float>(screenWidth) - m_width - m_screenMargin);
        const float menuHeight = static_cast<float>(m_commands.size()) * m_actionHeight +
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
        m_actor.reset();
        m_commands.clear();
    }
}
