#include "gameplay/work/WorkUtils.h"

namespace Infinitesia
{
    namespace WorkInternal
    {
        bool HasOrderForWorker(const std::vector<WorkSystem::WorkOrder>& orders, WorldObjectId worker)
        {
            for (const WorkSystem::WorkOrder& order : orders)
            {
                if (order.worker == worker)
                {
                    return true;
                }
            }
            return false;
        }

        bool HasOrderForTree(const std::vector<WorkSystem::WorkOrder>& orders, const std::vector<GridCell>& wood)
        {
            for (const WorkSystem::WorkOrder& order : orders)
            {
                if (order.type != WorkSystem::WorkType::ChopTree)
                {
                    continue;
                }
                for (const GridCell& cell : wood)
                {
                    if (cell.x == order.targetCell.x && cell.y == order.targetCell.y && cell.z == order.targetCell.z)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool HasOrderForDigCell(const std::vector<WorkSystem::WorkOrder>& orders, GridCell cell)
        {
            for (const WorkSystem::WorkOrder& order : orders)
            {
                if (order.type == WorkSystem::WorkType::DigBlock &&
                    order.targetCell.x == cell.x &&
                    order.targetCell.y == cell.y &&
                    order.targetCell.z == cell.z)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsReserved(const std::vector<WorkSystem::WorkOrder>& orders, GridCell cell)
        {
            for (const WorkSystem::WorkOrder& order : orders)
            {
                if (order.workCell.x == cell.x && order.workCell.y == cell.y &&
                    order.workCell.z == cell.z)
                {
                    return true;
                }
            }
            return false;
        }

        float GetWorkDuration(const WorkerConfig& config, const WorkSystem::WorkOrder& order)
        {
            switch (order.type)
            {
            case WorkSystem::WorkType::ChopTree:
                return config.choppingDuration;
            case WorkSystem::WorkType::DigBlock:
                return config.diggingDuration;
            case WorkSystem::WorkType::MoveToCell:
                return 0.0f;
            }
            return 0.0f;
        }
    }
}
