#pragma once
#include "ICommand.h"
#include <vector>
#include <memory>
#include <cstddef>

namespace StudioCore {

class CommandHistory {
public:
    CommandHistory() = default;

    void ExecuteCommand(std::unique_ptr<ICommand> command);
    void Undo();
    void Redo();
    void Clear();

    // Commands executed between BeginGroup and EndGroup become a single undo step.
    void BeginGroup();
    void EndGroup();

private:
    static constexpr std::size_t MaxUndoSteps = 100;

    void PushUndo(std::unique_ptr<ICommand> command);

    std::vector<std::unique_ptr<ICommand>> m_undoStack;
    std::vector<std::unique_ptr<ICommand>> m_redoStack;
    std::vector<std::unique_ptr<ICommand>> m_openGroup;
    int m_groupDepth{0};
};

}
