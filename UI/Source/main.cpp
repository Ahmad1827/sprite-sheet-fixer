#include "StudioEngineFacade.h"
#include "Systems/WorkspaceManager.h"
#include "DataModels/Project.h"
#include "DataModels/SpriteDefinition.h"
#include "DataModels/AnimationGroup.h"
#include "DataModels/ExportSettings.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "--- Testing Milestone 1 Foundation ---" << std::endl;

    // 1. Initialize Engine Facade
    StudioCore::StudioEngineFacade engine;
    engine.Initialize();

    if (engine.IsProjectActive()) {
        std::cerr << "FAILED: Engine should not have an active project before creation." << std::endl;
        return 1;
    }

    // 2. Create Project
    engine.CreateProject();
    if (!engine.IsProjectActive()) {
        std::cerr << "FAILED: Project creation failed." << std::endl;
        return 1;
    }
    std::cout << "[✓] Project instantiated via StudioEngineFacade." << std::endl;

    // 3. Access Workspace & Active Project
    auto workspace = engine.GetWorkspace();
    auto project = workspace->GetActiveProject();

    // 4. Test SpriteDefinition & Project insertion
    StudioCore::Rect rect{0, 0, 64, 64};
    auto sprite = std::make_shared<StudioCore::SpriteDefinition>("sprite_001", rect);
    sprite->SetPivot({0.5f, 1.0f});
    sprite->SetBaseline(58.0f);
    project->AddSprite(sprite);

    if (project->GetSprites().size() != 1) {
        std::cerr << "FAILED: Sprite not added to project." << std::endl;
        return 1;
    }
    std::cout << "[✓] SpriteDefinition instantiated and added to Project." << std::endl;

    // 5. Test AnimationGroup
    auto animGroup = std::make_shared<StudioCore::AnimationGroup>("Idle");
    animGroup->SetFps(12);
    animGroup->AddFrame(sprite);
    project->AddAnimationGroup(animGroup);

    if (project->GetAnimationGroups().size() != 1 || animGroup->GetFrames().size() != 1) {
        std::cerr << "FAILED: AnimationGroup test failed." << std::endl;
        return 1;
    }
    std::cout << "[✓] AnimationGroup created with frame reference." << std::endl;

    // 6. Test ExportSettings
    auto& settings = project->GetExportSettings();
    settings.cellWidth = 64;
    settings.cellHeight = 64;
    if (settings.cellWidth != 64 || settings.cellHeight != 64) {
        std::cerr << "FAILED: ExportSettings mutation failed." << std::endl;
        return 1;
    }
    std::cout << "[✓] ExportSettings configured." << std::endl;

    std::cout << "\n>>> MILESTONE 1 VERIFICATION PASSED SUCCESSFULLY! <<<\n" << std::endl;
    return 0;
}