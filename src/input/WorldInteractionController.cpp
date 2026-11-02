#include "input/WorldInteractionController.h"

namespace Infinitesia
{
    void WorldInteractionController::Update(
        WorldInteractionInput input,
        const Grid& grid,
        World& world,
        const Camera3D& camera,
        const CommandRegistry& commands,
        int screenWidth,
        int screenHeight)
    {
        m_hoveredCell = m_gridPointer.PickCell(grid, camera, input.mousePosition);
        if (m_contextMenu.BlocksWorldInput(input.mousePosition))
        {
            m_hoveredObject.reset();
        }
        else
        {
            m_hoveredObject = m_worldObjectPicker.PickObject(
                grid, world, camera, input.mousePosition);
        }

        std::optional<WorldObjectId> actor = m_selectedObject;
        if (actor.has_value() && world.FindObject(*actor) == nullptr)
        {
            actor.reset();
        }

        std::vector<CommandOption> targetCommands;
        if (m_hoveredObject.has_value())
        {
            targetCommands = commands.GetOptions(
                *m_hoveredObject,
                actor,
                world);
        }

        const ContextMenuUpdate menuUpdate = m_contextMenu.Update(
            input.mousePosition,
            input.rightClickPressed,
            input.leftClickPressed,
            input.escapePressed,
            m_hoveredObject,
            actor,
            targetCommands,
            screenWidth,
            screenHeight);

        if (menuUpdate.commandRequest.has_value())
        {
            const CommandRequest& request = *menuUpdate.commandRequest;
            const CommandResult result = commands.Execute(
                request.commandId,
                request.actor,
                request.target,
                world);
            m_actionNotice = result.message;
        }
        else if (input.leftClickPressed && !menuUpdate.consumedMouseClick)
        {
            m_selectedObject = m_hoveredObject;
            m_actionNotice.clear();
        }
    }

    std::optional<GridCell> WorldInteractionController::GetHoveredCell() const
    {
        return m_hoveredCell;
    }

    std::optional<WorldObjectId> WorldInteractionController::GetHoveredObject() const
    {
        return m_hoveredObject;
    }

    std::optional<WorldObjectId> WorldInteractionController::GetSelectedObject() const
    {
        return m_selectedObject;
    }

    const std::string& WorldInteractionController::GetActionNotice() const
    {
        return m_actionNotice;
    }

    void WorldInteractionController::DrawOverlay() const
    {
        m_contextMenu.Draw();
    }
}
