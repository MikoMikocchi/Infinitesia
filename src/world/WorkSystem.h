#pragma once

#include "world/Grid.h"
#include "world/GridPathfinder.h"
#include "world/World.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Infinitesia
{
    struct WorkerConfig
    {
        float movementSpeed;
        float choppingDuration;
        int logsPerTree;
    };

    enum class WorkOrderResult
    {
        Accepted,
        InvalidWorker,
        InvalidTarget,
        WorkerBusy,
        TargetBusy,
        NoReachableWorkCell
    };

    enum class WorkerActivity
    {
        Idle,
        Moving,
        Working
    };

    class WorkSystem
    {
    public:
        explicit WorkSystem(WorkerConfig config);

        WorkOrderResult AssignChopOrder(WorldObjectId worker, WorldObjectId tree, World& world, const Grid& grid);
        void Tick(World& world, const Grid& grid, float fixedDeltaTime);
        WorkerActivity GetActivity(WorldObjectId worker) const;
        float GetWorkProgress(WorldObjectId worker) const;
        std::vector<GridCell> GetRemainingPath(WorldObjectId worker) const;

    private:
        enum class WorkPhase
        {
            Moving,
            Chopping
        };

        struct WorkOrder
        {
            WorldObjectId worker;
            WorldObjectId tree;
            GridCell workCell;
            std::vector<GridCell> path;
            std::size_t nextPathCell = 1;
            WorkPhase phase;
            float choppingProgress = 0.0f;
        };

        bool HasOrderForWorker(WorldObjectId worker) const;
        bool HasOrderForTree(WorldObjectId tree) const;
        bool IsReserved(GridCell cell) const;
        std::optional<std::vector<GridCell>> FindPath(
            GridCell start,
            GridCell goal,
            WorldObjectId worker,
            const World& world,
            const Grid& grid) const;
        bool AdvanceOrder(WorkOrder& order, World& world, const Grid& grid, float fixedDeltaTime);
        bool AdvanceMovement(WorkOrder& order, World& world, const Grid& grid, float fixedDeltaTime);

        WorkerConfig m_config;
        GridPathfinder m_pathfinder;
        std::vector<WorkOrder> m_orders;
    };
}
