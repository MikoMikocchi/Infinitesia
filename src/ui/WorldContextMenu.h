#pragma once

#include "raylib.h"
#include "commands/Command.h"

#include <optional>
#include <string>
#include <vector>

namespace Infinitesia
{
    struct ContextMenuUpdate
    {
        bool consumedMouseClick = false;
        std::optional<CommandRequest> commandRequest;
        struct CellCommandRequest
        {
            std::string commandId;
            std::optional<WorldObjectId> actor;
            GridCell target;
        };
        std::optional<CellCommandRequest> cellCommandRequest;
    };

    class WorldContextMenu
    {
    public:
        ContextMenuUpdate Update(
            Vector2 mousePosition,
            bool rightClickPressed,
            bool leftClickPressed,
            bool escapePressed,
            std::optional<WorldObjectId> hoveredTarget,
            std::optional<GridCell> hoveredCell,
            std::optional<WorldObjectId> actor,
            const std::vector<CommandOption>& targetCommands,
            const std::vector<CommandOption>& cellCommands,
            int screenWidth,
            int screenHeight);

        void Draw() const;
        bool BlocksWorldInput(Vector2 mousePosition) const;

    private:
        static constexpr float m_width = 220.0f;
        static constexpr float m_actionHeight = 36.0f;
        static constexpr float m_disabledHintHeight = 22.0f;
        static constexpr float m_screenMargin = 8.0f;
        static constexpr float m_mouseOffset = 6.0f;

        Rectangle GetBounds() const;
        Rectangle GetActionBounds(std::size_t index) const;
        const char* GetDisabledHint() const;
        bool HasDisabledCommands() const;
        void Open(
            Vector2 position,
            std::optional<WorldObjectId> target,
            std::optional<GridCell> targetCell,
            std::optional<WorldObjectId> actor,
            const std::vector<CommandOption>& commands,
            int screenWidth,
            int screenHeight);
        void Close();

        bool m_isOpen = false;
        Vector2 m_position{ 0.0f, 0.0f };
        std::optional<WorldObjectId> m_target;
        std::optional<GridCell> m_targetCell;
        std::optional<WorldObjectId> m_actor;
        std::vector<CommandOption> m_commands;
    };
}
