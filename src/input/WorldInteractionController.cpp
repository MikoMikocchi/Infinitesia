#include "input/WorldInteractionController.h"

namespace Infinitesia
{
    namespace
    {
        std::optional<WorldObjectId> GetObjectOfType(
            const World& world,
            std::optional<WorldObjectId> objectId,
            WorldObjectType type)
        {
            if (!objectId.has_value())
            {
                return std::nullopt;
            }

            const WorldObject* object = world.FindObject(*objectId);
            if (object == nullptr || object->type != type)
            {
                return std::nullopt;
            }
            return object->id;
        }
    }

    void WorldInteractionController::Update(
        WorldInteractionInput input,
        const Grid& grid,
        World& world,
        const Camera3D& camera,
        const WorkerCommandRegistry& commands,
        WorkSystem& workSystem,
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

        const std::optional<WorldObjectId> selectedWorker =
            GetObjectOfType(world, m_selectedObject, WorldObjectType::Worker);
        std::vector<WorkerCommandOption> targetCommands;
        if (m_hoveredObject.has_value())
        {
            targetCommands = commands.GetOptions(
                *m_hoveredObject,
                selectedWorker,
                world,
                grid,
                workSystem);
        }

        const ContextMenuUpdate menuUpdate = m_contextMenu.Update(
            input.mousePosition,
            input.rightClickPressed,
            input.leftClickPressed,
            input.escapePressed,
            m_hoveredObject,
            selectedWorker,
            targetCommands,
            screenWidth,
            screenHeight);

        if (menuUpdate.commandRequest.has_value())
        {
            const WorkerCommandRequest& request = *menuUpdate.commandRequest;
            const WorkerCommandResult result = commands.Execute(
                request.commandId,
                request.actor,
                request.target,
                world,
                grid,
                workSystem);
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
