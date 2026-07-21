#include "DataModels/SpriteDefinition.h"
#include <utility>

namespace StudioCore {

SpriteDefinition::SpriteDefinition(std::string id, const Rect& sourceRect)
    : m_id(std::move(id)), m_sourceRect(sourceRect) {}

const std::string& SpriteDefinition::GetId() const { 
    return m_id; 
}

const Rect& SpriteDefinition::GetSourceRect() const { 
    return m_sourceRect; 
}

void SpriteDefinition::SetPivot(const Point& pivot) { 
    m_pivot = pivot; 
}

const Point& SpriteDefinition::GetPivot() const { 
    return m_pivot; 
}

void SpriteDefinition::SetBaseline(float baseline) { 
    m_baseline = baseline; 
}

float SpriteDefinition::GetBaseline() const { 
    return m_baseline; 
}

}