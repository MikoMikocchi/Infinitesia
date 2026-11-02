#pragma once

#include "world/Command.h"

namespace Infinitesia
{
    class WorkSystem;

    class ChopTreeCommand final : public Command
    {
    public:
        ChopTreeCommand(WorkSystem& workSystem, const Grid& grid);

        std::string_view GetId() const override;
        std::string_view GetLabel() const override;
        bool IsApplicable(const WorldObject& target) const override;
        std::string GetDisabledReason(
            std::optional<WorldObjectId> actor,
            const WorldObject& target,
            const World& world) const override;
        CommandResult Execute(
            std::optional<WorldObjectId> actor,
            WorldObjectId target,
            World& world) const override;

    private:
        WorkSystem& m_workSystem;
        const Grid& m_grid;
    };
}
