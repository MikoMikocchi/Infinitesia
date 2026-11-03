#pragma once

#include "presentation/assets/TextureAssetManager.h"
#include "presentation/input/GridPointer.h"
#include "presentation/input/WorldObjectPicker.h"
#include "presentation/rendering/ViewMode.h"
#include "raylib.h"
#include "presentation/ui/CharacterProfilePanel.h"
#include "presentation/ui/WorldContextMenu.h"
#include "gameplay/GameWorld.h"
#include "gameplay/commands/CommandRegistry.h"
#include "world/Grid.h"
#include "gameplay/work/WorkSystem.h"

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
            GameWorld& world,
            WorkSystem& workSystem,
            const Camera3D& camera,
            const CommandRegistry& commands,
            int screenWidth,
            int screenHeight,
            int selectedLevel,
            ViewMode viewMode);

        std::optional<GridCell> GetHoveredCell() const;
        std::optional<WorldObjectId> GetHoveredObject() const;
        std::optional<WorldObjectId> GetSelectedObject() const;
        const std::string& GetActionNotice() const;
        void DrawOverlay(const EntityStore& world, const TextureAssetManager& assets) const;

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
