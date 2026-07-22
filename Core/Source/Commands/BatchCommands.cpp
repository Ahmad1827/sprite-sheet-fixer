#include "Commands/BatchCommands.h"
#include "DataModels/Project.h"
#include "DataModels/SpriteDefinition.h"

namespace StudioCore {

BatchOperationCommand::BatchOperationCommand(std::shared_ptr<Project> project, const std::vector<std::string>& spriteIds, BatchOp op)
    : m_project(project), m_spriteIds(spriteIds), m_op(op) {}

void BatchOperationCommand::Execute() {
    if (!m_project) return;
    float avgBaseline = 0.0f;
    if (m_op == BatchOp::NormalizeBaseline) {
        int count = 0;
        for (const auto& id : m_spriteIds) {
            auto s = m_project->GetSpriteById(id);
            if (s) {
                avgBaseline += s->GetBaseline();
                count++;
            }
        }
        if (count > 0) avgBaseline /= count;
    }

    for (const auto& id : m_spriteIds) {
        auto s = m_project->GetSpriteById(id);
        if (!s) continue;

        if (m_op == BatchOp::FlipHorizontal) {
            Point p = s->GetPivot();
            p.x = 1.0f - p.x;
            s->SetPivot(p);
        } else if (m_op == BatchOp::FlipVertical) {
            Point p = s->GetPivot();
            p.y = 1.0f - p.y;
            s->SetPivot(p);
        } else if (m_op == BatchOp::ResetPivot) {
            s->SetPivot({0.5f, 0.5f});
        } else if (m_op == BatchOp::ResetBaseline) {
            s->SetBaseline(static_cast<float>(s->GetSourceRect().height));
        } else if (m_op == BatchOp::NormalizeBaseline) {
            s->SetBaseline(avgBaseline);
        }
    }
}

void BatchOperationCommand::Undo() {
}

AlignSpritesCommand::AlignSpritesCommand(std::shared_ptr<Project> project, const std::vector<std::string>& spriteIds, AlignOp op)
    : m_project(project), m_spriteIds(spriteIds), m_op(op) {}

void AlignSpritesCommand::Execute() {
}

void AlignSpritesCommand::Undo() {
}

}