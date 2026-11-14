#include "app/native_results_ui.h"
#include "app/app_input.h"
#include "app/menu_controller.h"
#include "app/menu_ui.h"
#include "app/rml_ui_text.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <iomanip>
#include <sstream>
#include <cstddef>

namespace locus::app {
namespace {
std::string Escape(const std::string& source) {
    std::string out;
    for (const char c : source) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}
std::string Percent(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value << '%';
    return out.str();
}
}
void NativeResultsUi::Attach(Rml::ElementDocument* document) noexcept {
    document_ = document;
    textCache_.clear();
    lastHistoryRevision_ = ~std::uint64_t(0);
    lastSelectedId_.clear();
}
void NativeResultsUi::BeginFrame(
    const RunResult* selected, const core::GradeCounts& grades,
    int total, const std::string& title, bool historical,
    const SongSelectUi& history, FrameInput& input,
    MenuEvents& events) noexcept {
    selected_ = selected;
    grades_ = &grades;
    total_ = total;
    chartTitle_ = title;
    historical_ = historical;
    history_ = &history;
    input_ = &input;
    events_ = &events;
}
void NativeResultsUi::EndFrame() noexcept {
    selected_ = nullptr;
    grades_ = nullptr;
    history_ = nullptr;
    input_ = nullptr;
    events_ = nullptr;
}
void NativeResultsUi::SetText(const char* id, const std::string& value) {
    if (!document_) return;
    auto it = textCache_.find(id);
    if (it != textCache_.end() && it->second == value) return;
    if (auto* element = document_->GetElementById(id)) {
        UpdateRmlText(element, value);
        textCache_[id] = value;
    }
}
void NativeResultsUi::Sync() {
    if (!document_ || !grades_ || !history_) return;
    SetText("results-heading", historical_ ? "SAVED RESULT" : "CHART COMPLETE");
    SetText("results-song", selected_ ? selected_->songTitle : "LOCUS");
    SetText("results-chart", selected_ ? selected_->chartTitle : chartTitle_);
    SetText("results-date", selected_
        ? FormatRunTimestamp(selected_->completedAtMs) : "");
    SetText("results-accuracy", Percent(selected_ ?
        selected_->accuracyPercent : grades_->AccuracyPercent()));
    SetText("results-notes", std::to_string(grades_->Resolved()) +
        " / " + std::to_string(total_));
    SetText("results-perfect", std::to_string(grades_->perfect));
    SetText("results-good", std::to_string(grades_->good));
    SetText("results-bad", std::to_string(grades_->bad));
    SetText("results-miss", std::to_string(grades_->miss));
    SetText("results-practice", selected_ && selected_->practiceRate < 0.999
        ? "PRACTICE " + std::to_string(static_cast<int>(
            selected_->practiceRate * 100 + 0.5)) + "%" : "");
    for (const char* id : {"results-retry", "results-edit"}) {
        if (auto* button = document_->GetElementById(id))
            button->SetProperty("display", historical_ ? "none" : "inline-block");
    }
    const std::string selectedId = selected_ ? selected_->id : "";
    if (lastHistoryRevision_ == history_->resultsRevision &&
        lastSelectedId_ == selectedId) return;
    lastHistoryRevision_ = history_->resultsRevision;
    lastSelectedId_ = selectedId;
    if (auto* list = document_->GetElementById("results-history")) {
        std::ostringstream html;
        // Do not offer results from a previously selected Song Set.
        const bool sameChart = selected_ &&
            history_->resultKey.songSetId == selected_->songSetId &&
            history_->resultKey.difficultyFile == selected_->difficultyFile;
        if (sameChart) {
            for (std::size_t i = 0; i < history_->results.size(); ++i) {
                const auto& run = history_->results[i];
                if (run.id == selectedId) continue;
                html << "<button class='results-history-row' data-action='history'"
                     << " data-index='" << i << "'><span class='button-label'>"
                     << Escape(Percent(run.accuracyPercent)) << " / "
                     << Escape(FormatRunTimestamp(run.completedAtMs))
                     << "</span></button>";
            }
        }
        if (html.str().empty())
            html << "<div class='results-empty'>NO OTHER SAVED RESULTS</div>";
        list->SetInnerRML(html.str());
    }
}
void NativeResultsUi::ProcessEvent(Rml::Event& event) {
    if (!document_ || !input_ || !events_ || event.GetType() != "click") return;
    auto* target = event.GetTargetElement();
    while (target && !target->HasAttribute("data-action"))
        target = target->GetParentNode();
    if (!target || target->HasAttribute("disabled")) return;
    const std::string action = target->GetAttribute<Rml::String>("data-action", "");
    if (action == "back") input_->backPressed = true;
    else if (!historical_ && action == "retry") input_->startPressed = true;
    else if (!historical_ && action == "edit") input_->openEditorPressed = true;
    else if (action == "history") {
        const int index = target->GetAttribute<int>("data-index", -1);
        if (history_ && index >= 0 &&
            index < static_cast<int>(history_->results.size()))
            events_->viewResultIndex = index;
    }
}
} // namespace locus::app
