#pragma once

#include "input/GridPointer.h"
#include "input/WorldObjectPicker.h"
#include "raylib.h"
#include "ui/CharacterProfilePanel.h"
#include "ui/WorldContextMenu.h"
#include "commands/CommandRegistry.h"
#include "world/Grid.h"
#include "world/WorkSystem.h"

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
            WorkSystem& workSystem,
            const Camera3D& camera,
            const CommandRegistry& commands,
            int screenWidth,
            int screenHeight,
            int selectedLevel);

        std::optional<GridCell> GetHoveredCell() const;
        std::optional<WorldObjectId> GetHoveredObject() const;
        std::optional<WorldObjectId> GetSelectedObject() const;
        const std::string& GetActionNotice() const;
        void DrawOverlay(const World& world) const;

    private:
        GridPointer m_gridPointer;
        WorldObjectPicker m_worldObjectPicker;
        WorldContextMenu m_contextMenu;
        CharacterProfilePanel m_characterProfilePanel;
        std::optional<GridCell> m_hoveredCell;
        std::optional<WorldObjectId> m_hoveredObject;
        std::optional<WorldObjectId> m_selectedObject;
        std::optional<WorldObjectId> m_lastClickedObject;
        double m_lastObjectClickTime = -1.0;
        std::string m_actionNotice;
    };
}
