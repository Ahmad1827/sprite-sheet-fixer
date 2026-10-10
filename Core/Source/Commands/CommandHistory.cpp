#include "Commands/CommandHistory.h"

namespace StudioCore {

namespace {

class CommandGroup : public ICommand {
public:
    explicit CommandGroup(std::vector<std::unique_ptr<ICommand>> commands)
        : m_commands(std::move(commands)) {}

    void Execute() override {
        for (auto& command : m_commands) command->Execute();
    }

    void Undo() override {
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) (*it)->Undo();
    }

private:
    std::vector<std::unique_ptr<ICommand>> m_commands;
};

}

void CommandHistory::ExecuteCommand(std::unique_ptr<ICommand> command) {
    command->Execute();
    if (m_groupDepth > 0) {
        m_openGroup.push_back(std::move(command));
        return;
    }
    PushUndo(std::move(command));
}

void CommandHistory::Undo() {
    if (m_groupDepth > 0 || m_undoStack.empty()) return;
    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->Undo();
    m_redoStack.push_back(std::move(command));
}

void CommandHistory::Redo() {
    if (m_groupDepth > 0 || m_redoStack.empty()) return;
    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->Execute();
    m_undoStack.push_back(std::move(command));
}

void CommandHistory::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_openGroup.clear();
    m_groupDepth = 0;
}

void CommandHistory::BeginGroup() {
    ++m_groupDepth;
}

void CommandHistory::EndGroup() {
    if (m_groupDepth == 0) return;
    if (--m_groupDepth > 0) return;

    if (m_openGroup.empty()) return;
    if (m_openGroup.size() == 1) {
        PushUndo(std::move(m_openGroup.front()));
    } else {
        PushUndo(std::make_unique<CommandGroup>(std::move(m_openGroup)));
    }
    m_openGroup.clear();
}

void CommandHistory::PushUndo(std::unique_ptr<ICommand> command) {
    m_undoStack.push_back(std::move(command));
    m_redoStack.clear();
    if (m_undoStack.size() > MaxUndoSteps) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

}
