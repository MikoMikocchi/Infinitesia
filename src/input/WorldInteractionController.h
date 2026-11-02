#pragma once

#include "input/GridPointer.h"
#include "input/WorldObjectPicker.h"
#include "raylib.h"
#include "ui/WorldContextMenu.h"
#include "commands/CommandRegistry.h"
#include "world/Grid.h"

#include <optional>
#include <string>

namespace Infinitesia
{
    struct WorldInteractionInput
    {
        Vector2 mousePosition;
        bool rightClickPressed;
        bool leftClickPressed;
        bool escapePressed;
    };

    class WorldInteractionController
    {
    public:
        void Update(
            WorldInteractionInput input,
            const Grid& grid,
            World& world,
            const Camera3D& camera,
            const CommandRegistry& commands,
            int screenWidth,
            int screenHeight);

        std::optional<GridCell> GetHoveredCell() const;
        std::optional<WorldObjectId> GetHoveredObject() const;
        std::optional<WorldObjectId> GetSelectedObject() const;
        const std::string& GetActionNotice() const;
        void DrawOverlay() const;

    private:
        GridPointer m_gridPointer;
        WorldObjectPicker m_worldObjectPicker;
        WorldContextMenu m_contextMenu;
        std::optional<GridCell> m_hoveredCell;
        std::optional<WorldObjectId> m_hoveredObject;
        std::optional<WorldObjectId> m_selectedObject;
        std::string m_actionNotice;
    };
}
