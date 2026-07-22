#pragma once
#include "Commands/ICommand.h"
#include "DataModels/SpriteDefinition.h"
#include <vector>
#include <memory>

namespace StudioCore {

struct SpriteMoveState {
    std::shared_ptr<SpriteDefinition> sprite;
    sf::FloatRect oldRect;
    sf::FloatRect newRect;
};

class MoveSpriteCommand : public ICommand {
public:
    explicit MoveSpriteCommand(std::vector<SpriteMoveState> moves)
        : m_moves(std::move(moves)) {}

    void Execute() override {
        for (auto& move : m_moves) {
            if (move.sprite) {
                move.sprite->SetSourceRect(move.newRect);
            }
        }
    }

    void Undo() override {
        for (auto& move : m_moves) {
            if (move.sprite) {
                move.sprite->SetSourceRect(move.oldRect);
            }
        }
    }

private:
    std::vector<SpriteMoveState> m_moves;
};

}