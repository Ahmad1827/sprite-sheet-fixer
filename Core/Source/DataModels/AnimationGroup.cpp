#include "DataModels/AnimationGroup.h"
#include "DataModels/SpriteDefinition.h"
#include <utility>

namespace StudioCore {

AnimationGroup::AnimationGroup(std::string name) 
    : m_name(std::move(name)) {}

const std::string& AnimationGroup::GetName() const { 
    return m_name; 
}

void AnimationGroup::SetFps(int fps) { 
    m_fps = fps; 
}

int AnimationGroup::GetFps() const { 
    return m_fps; 
}

void AnimationGroup::SetLooping(bool looping) { 
    m_looping = looping; 
}

bool AnimationGroup::IsLooping() const { 
    return m_looping; 
}

void AnimationGroup::AddFrame(std::shared_ptr<SpriteDefinition> sprite) {
    if (sprite) {
        m_frames.push_back(std::move(sprite));
    }
}

void AnimationGroup::RemoveFrame(size_t index) {
    if (index < m_frames.size()) {
        m_frames.erase(m_frames.begin() + index);
    }
}

const std::vector<std::shared_ptr<SpriteDefinition>>& AnimationGroup::GetFrames() const {
    return m_frames;
}

}