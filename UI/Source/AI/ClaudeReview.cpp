#include "AI/ClaudeReview.h"
#include "StudioEngineFacade.h"
#include "DataModels/SourceTexture.h"
#include "Utils/ProcessRunner.h"
#include "json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace StudioUI {

using json = nlohmann::json;

namespace {

std::string Base64(const std::vector<sf::Uint8>& data) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        const unsigned b0 = data[i];
        const unsigned b1 = i + 1 < data.size() ? data[i + 1] : 0;
        const unsigned b2 = i + 2 < data.size() ? data[i + 2] : 0;
        out += table[b0 >> 2];
        out += table[((b0 & 3) << 4) | (b1 >> 4)];
        out += i + 1 < data.size() ? table[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        out += i + 2 < data.size() ? table[b2 & 63] : '=';
    }
    return out;
}

std::string FirstLine(const std::string& text) {
    std::string line = text.substr(0, text.find('\n'));
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line.size() > 160) line = line.substr(0, 160) + "...";
    return line;
}

std::string TempPath(const std::string& name) {
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
    if (ec) dir = ".";
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return (dir / ("spritesheetfixer_" + std::to_string(stamp) + "_" + name)).string();
}

}

std::string ClaudeReview::TestCommand(const std::string& command) {
    const std::string out = TempPath("version.txt");
    const std::string err = TempPath("version_err.txt");
    const int code = ProcessRunner::Run(command + " --version", "", out, err);
    std::string version = FirstLine(ProcessRunner::ReadFile(out));
    std::error_code ec;
    std::filesystem::remove(out, ec);
    std::filesystem::remove(err, ec);
    if (code != 0 || version.empty()) return "";
    return version;
}

std::string ClaudeReview::BuildPrompt(const AppSettings& settings) const {
    std::string targets;
    if (settings.aiRemoveLabels) targets += "text labels, captions, numbers and watermarks";
    if (settings.aiRemoveDebris) {
        if (!targets.empty()) targets += "; ";
        targets += "stray specks, smears, frame lines and leftover pieces of background";
    }
    if (targets.empty()) targets = "anything the instructions below ask for";

    std::string prompt =
        "This is a game sprite sheet whose background has already been removed; it is shown on a flat grey backdrop. "
        "Every separate object has a pink box with its number in a pink tag on the top-left corner. "
        "Decide which numbered boxes are NOT game art and should be deleted: " + targets + ". "
        "Keep every real sprite, including small ones such as pebbles, leaves, sparks or particles. "
        "If you are unsure about a box, keep it.";
    if (!settings.aiExtraInstructions.empty()) {
        prompt += " Extra instructions from the user: " + settings.aiExtraInstructions;
    }
    prompt += " Answer with only JSON in this exact form: {\"remove\":[numbers]}";
    return prompt;
}

sf::Image ClaudeReview::RenderOverview(const StudioCore::StudioEngineFacade& engine, const sf::Font& font) const {
    auto tex = engine.GetCurrentTexture();
    const float w = static_cast<float>(tex->GetWidth());
    const float h = static_cast<float>(tex->GetHeight());
    const float scale = std::min(1.0f, 1568.0f / std::max(w, h));
    const unsigned outW = std::max(1u, static_cast<unsigned>(w * scale));
    const unsigned outH = std::max(1u, static_cast<unsigned>(h * scale));

    sf::Texture sheet;
    sf::RenderTexture target;
    if (!sheet.create(tex->GetWidth(), tex->GetHeight()) || !target.create(outW, outH)) return sf::Image();
    sheet.update(tex->GetPixels().data());
    sheet.setSmooth(true);

    target.clear(sf::Color(58, 60, 70));
    sf::Sprite sprite(sheet);
    sprite.setScale(scale, scale);
    target.draw(sprite);

    const sf::Color pink(255, 80, 200);
    for (size_t i = 0; i < m_regions.size(); ++i) {
        const auto& r = m_regions[i];
        sf::RectangleShape box({ r.width * scale + 4.0f, r.height * scale + 4.0f });
        box.setPosition(std::floor(r.x * scale - 2.0f), std::floor(r.y * scale - 2.0f));
        box.setFillColor(sf::Color::Transparent);
        box.setOutlineThickness(1.0f);
        box.setOutlineColor(pink);
        target.draw(box);

        sf::Text number(std::to_string(i + 1), font, 12);
        number.setStyle(sf::Text::Bold);
        number.setFillColor(sf::Color::Black);
        sf::FloatRect nb = number.getLocalBounds();
        sf::RectangleShape tag({ nb.width + 7.0f, 15.0f });
        tag.setPosition(box.getPosition().x, std::max(0.0f, box.getPosition().y - 15.0f));
        tag.setFillColor(pink);
        target.draw(tag);
        number.setPosition(std::floor(tag.getPosition().x + 3.0f - nb.left), std::floor(tag.getPosition().y + 1.0f - nb.top + 1.0f));
        target.draw(number);
    }
    target.display();
    return target.getTexture().copyToImage();
}

void ClaudeReview::Fail(const std::string& message) {
    m_state = State::Failed;
    m_error = message;
    m_job.reset();
}

void ClaudeReview::Start(const StudioCore::StudioEngineFacade& engine, const AppSettings& settings, const sf::Font& font) {
    Cancel();
    m_command = settings.aiCommand.empty() ? "claude" : settings.aiCommand;

    if (!engine.HasTexture()) {
        Fail("Open an image first.");
        return;
    }
    m_regions = engine.FindLooseRegions();
    m_marked.assign(m_regions.size(), false);
    if (m_regions.empty()) {
        Fail("There is nothing on the sheet to review.");
        return;
    }

    sf::Image overview = RenderOverview(engine, font);
    std::vector<sf::Uint8> png;
    if (overview.getSize().x == 0 || !overview.saveToMemory(png, "png")) {
        Fail("Could not prepare the picture to send.");
        return;
    }

    json message;
    message["type"] = "user";
    message["message"]["role"] = "user";
    message["message"]["content"] = json::array({
        { { "type", "image" }, { "source", { { "type", "base64" }, { "media_type", "image/png" }, { "data", Base64(png) } } } },
        { { "type", "text" }, { "text", BuildPrompt(settings) } }
    });

    const std::string inputPath = TempPath("in.jsonl");
    const std::string outputPath = TempPath("out.jsonl");
    const std::string errorPath = TempPath("err.txt");
    {
        std::ofstream input(inputPath, std::ios::binary);
        if (!input.is_open()) {
            Fail("Could not write a temporary file.");
            return;
        }
        input << message.dump() << "\n";
    }

    // The picture goes in through standard input and no tools are allowed, so
    // Claude Code can only look and answer; it cannot touch any file.
    std::string command = m_command + " -p --input-format stream-json --output-format stream-json --verbose"
                                      " --tools \"\" --no-session-persistence";
    if (!settings.aiModel.empty() && settings.aiModel != "default") command += " --model " + settings.aiModel;

    auto job = std::make_shared<Job>();
    m_job = job;
    m_state = State::Running;
    m_error.clear();
    m_clock.restart();

    std::thread([job, command, inputPath, outputPath, errorPath]() {
        job->exitCode = ProcessRunner::Run(command, inputPath, outputPath, errorPath);
        job->output = ProcessRunner::ReadFile(outputPath);
        job->errors = ProcessRunner::ReadFile(errorPath);
        std::error_code ec;
        std::filesystem::remove(inputPath, ec);
        std::filesystem::remove(outputPath, ec);
        std::filesystem::remove(errorPath, ec);
        job->done = true;
    }).detach();
}

void ClaudeReview::Update() {
    if (m_state != State::Running || !m_job || !m_job->done) return;
    ReadAnswer();
}

void ClaudeReview::ReadAnswer() {
    std::shared_ptr<Job> job = m_job;
    m_job.reset();

    // The output is one JSON object per line; the one of type "result" has the answer.
    std::string answer;
    bool gotResult = false;
    bool isError = false;
    std::istringstream lines(job->output);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] != '{') continue;
        json j = json::parse(line, nullptr, false);
        if (j.is_discarded() || j.value("type", "") != "result") continue;
        gotResult = true;
        isError = j.value("is_error", false);
        if (j.contains("result") && j["result"].is_string()) answer = j["result"].get<std::string>();
    }

    if (!gotResult) {
        std::string detail = FirstLine(job->errors);
        if (detail.empty()) detail = FirstLine(job->output);
        Fail("Could not get an answer from \"" + m_command + "\". Is Claude Code installed and signed in? "
             "You can change the command in Settings." + (detail.empty() ? "" : "\n" + detail));
        return;
    }
    if (isError) {
        Fail("Claude Code reported a problem:\n" + FirstLine(answer));
        return;
    }

    const size_t open = answer.find('{');
    const size_t close = answer.rfind('}');
    json verdict = (open != std::string::npos && close != std::string::npos && close > open)
                       ? json::parse(answer.substr(open, close - open + 1), nullptr, false)
                       : json(json::value_t::discarded);
    if (verdict.is_discarded() || !verdict.contains("remove") || !verdict["remove"].is_array()) {
        Fail("Claude answered in a form the app could not read:\n" + FirstLine(answer));
        return;
    }

    m_marked.assign(m_regions.size(), false);
    for (const auto& item : verdict["remove"]) {
        if (!item.is_number_integer()) continue;
        const long long number = item.get<long long>();
        if (number >= 1 && number <= static_cast<long long>(m_regions.size())) m_marked[static_cast<size_t>(number - 1)] = true;
    }
    m_state = State::Choosing;
}

void ClaudeReview::Cancel() {
    // A run that is still going is simply left to finish on its own; its
    // answer is dropped.
    m_job.reset();
    m_regions.clear();
    m_marked.clear();
    m_error.clear();
    m_state = State::Idle;
}

int ClaudeReview::MarkedCount() const {
    return static_cast<int>(std::count(m_marked.begin(), m_marked.end(), true));
}

bool ClaudeReview::ToggleAt(sf::Vector2f imagePos) {
    // The smallest box under the cursor wins, so a caption inside a bigger box
    // can still be picked.
    int best = -1;
    long bestArea = 0;
    for (size_t i = 0; i < m_regions.size(); ++i) {
        const auto& r = m_regions[i];
        if (imagePos.x < r.x || imagePos.y < r.y || imagePos.x >= r.x + r.width || imagePos.y >= r.y + r.height) continue;
        const long area = static_cast<long>(r.width) * r.height;
        if (best < 0 || area < bestArea) {
            best = static_cast<int>(i);
            bestArea = area;
        }
    }
    if (best < 0) return false;
    m_marked[best] = !m_marked[best];
    return true;
}

std::vector<StudioCore::LooseRegion> ClaudeReview::TakeMarked() {
    std::vector<StudioCore::LooseRegion> marked;
    for (size_t i = 0; i < m_regions.size(); ++i) {
        if (m_marked[i]) marked.push_back(std::move(m_regions[i]));
    }
    Cancel();
    return marked;
}

}
