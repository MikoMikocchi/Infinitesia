#pragma once

#include "world/WorkerCommand.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Infinitesia
{
    class WorkerCommandRegistry
    {
    public:
        void Register(std::unique_ptr<WorkerCommand> command);

        std::vector<WorkerCommandOption> GetOptions(
            WorldObjectId target,
            std::optional<WorldObjectId> actor,
            const World& world,
            const Grid& grid,
            const WorkSystem& workSystem) const;

        WorkerCommandResult Execute(
            std::string_view commandId,
            WorldObjectId actor,
            WorldObjectId target,
            World& world,
            const Grid& grid,
            WorkSystem& workSystem) const;

    private:
        const WorkerCommand* FindCommand(std::string_view commandId) const;

        std::vector<std::unique_ptr<WorkerCommand>> m_commands;
    };
}
