#include "DataModels/Project.h"
#include "DataModels/SourceTexture.h"
#include "DataModels/SpriteDefinition.h"
#include "DataModels/AnimationGroup.h"
#include <algorithm>

namespace StudioCore {

void Project::SetTexture(std::shared_ptr<const SourceTexture> texture) {
    m_texture = std::move(texture);
}

std::shared_ptr<const SourceTexture> Project::GetTexture() const {
    return m_texture;
}

void Project::AddSprite(std::shared_ptr<SpriteDefinition> sprite) {
    if (sprite) {
        m_sprites.push_back(std::move(sprite));
    }
}

const std::vector<std::shared_ptr<SpriteDefinition>>& Project::GetSprites() const {
    return m_sprites;
}

std::shared_ptr<SpriteDefinition> Project::GetSpriteById(const std::string& id) const {
    for (const auto& sprite : m_sprites) {
        if (sprite->GetId() == id) return sprite;
    }
    return nullptr;
}

void Project::AddAnimationGroup(std::shared_ptr<AnimationGroup> group) {
    if (group) {
        m_animations.push_back(std::move(group));
    }
}

void Project::RemoveAnimationGroup(const std::string& id) {
    m_animations.erase(
        std::remove_if(m_animations.begin(), m_animations.end(), 
            [&id](const std::shared_ptr<AnimationGroup>& group) {
                return group->GetId() == id;
            }), 
        m_animations.end()
    );
}

const std::vector<std::shared_ptr<AnimationGroup>>& Project::GetAnimationGroups() const {
    return m_animations;
}

std::shared_ptr<AnimationGroup> Project::GetAnimationById(const std::string& id) const {
    for (const auto& anim : m_animations) {
        if (anim->GetId() == id) return anim;
    }
    return nullptr;
}

ExportSettings& Project::GetExportSettings() {
    return m_exportSettings;
}

const ExportSettings& Project::GetExportSettings() const {
    return m_exportSettings;
}

}