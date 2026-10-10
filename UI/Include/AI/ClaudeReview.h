#pragma once
#include <SFML/Graphics.hpp>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include "AppSettings.h"
#include "Processing/RegionFinder.h"

namespace StudioCore { class StudioEngineFacade; }

namespace StudioUI {

// Asks Claude Code which of the objects left on a cleaned sheet are not game art
// (captions, watermarks, stray bits). The app finds the objects and numbers them
// on a small picture; Claude only ever answers with numbers, so what gets deleted
// is always an exact object the app found itself.
class ClaudeReview {
public:
    enum class State { Idle, Running, Choosing, Failed };

    void Start(const StudioCore::StudioEngineFacade& engine, const AppSettings& settings, const sf::Font& font);
    void Update();   // call every frame; picks up the answer when it arrives
    void Cancel();

    State GetState() const { return m_state; }
    const std::string& GetError() const { return m_error; }
    float SecondsRunning() const { return m_clock.getElapsedTime().asSeconds(); }

    const std::vector<StudioCore::LooseRegion>& Regions() const { return m_regions; }
    bool IsMarked(size_t index) const { return index < m_marked.size() && m_marked[index]; }
    int MarkedCount() const;
    // Flips the object under an image position between keep and remove.
    bool ToggleAt(sf::Vector2f imagePos);
    // Hands back the objects to delete and returns to Idle.
    std::vector<StudioCore::LooseRegion> TakeMarked();

    // "<command> --version" for the Settings test button.
    static std::string TestCommand(const std::string& command);

private:
    struct Job {
        std::atomic<bool> done{false};
        std::string output;
        std::string errors;
        int exitCode{0};
    };

    std::string BuildPrompt(const AppSettings& settings) const;
    sf::Image RenderOverview(const StudioCore::StudioEngineFacade& engine, const sf::Font& font) const;
    void Fail(const std::string& message);
    void ReadAnswer();

    State m_state{State::Idle};
    std::string m_error;
    std::string m_command;
    std::vector<StudioCore::LooseRegion> m_regions;
    std::vector<bool> m_marked;
    std::shared_ptr<Job> m_job;
    sf::Clock m_clock;
};

}
