#pragma once

#include "world/WorkerCommand.h"

namespace Infinitesia
{
    class ChopTreeCommand final : public WorkerCommand
    {
    public:
        std::string_view GetId() const override;
        std::string_view GetLabel() const override;
        bool IsApplicable(const WorldObject& target) const override;
        std::string GetDisabledReason(
            std::optional<WorldObjectId> actor,
            const WorldObject& target,
            const World& world,
            const Grid& grid,
            const WorkSystem& workSystem) const override;
        WorkerCommandResult Execute(
            WorldObjectId actor,
            WorldObjectId target,
            World& world,
            const Grid& grid,
            WorkSystem& workSystem) const override;
    };
}
