#pragma once
#include "Commands/ICommand.h"
#include "DataModels/Project.h"
#include "DataModels/SpriteDefinition.h"
#include <memory>
#include <vector>

namespace StudioCore {

class ReplaceSpritesCommand : public ICommand {
public:
    ReplaceSpritesCommand(std::shared_ptr<Project> project,
                          const std::vector<std::shared_ptr<SpriteDefinition>>& oldSprites,
                          const std::vector<std::shared_ptr<SpriteDefinition>>& newSprites)
        : m_project(project), m_oldSprites(oldSprites), m_newSprites(newSprites) {}

    void Execute() override {
        if (m_project) m_project->SetSprites(m_newSprites);
    }

    void Undo() override {
        if (m_project) m_project->SetSprites(m_oldSprites);
    }

private:
    std::shared_ptr<Project> m_project;
    std::vector<std::shared_ptr<SpriteDefinition>> m_oldSprites;
    std::vector<std::shared_ptr<SpriteDefinition>> m_newSprites;
};

}
