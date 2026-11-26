#include "app/native_results_ui.h"
#include "app/app_input.h"
#include "app/menu_controller.h"
#include "app/menu_ui.h"
#include "app/rml_ui_text.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <iomanip>
#include <sstream>
#include <cstddef>
#include <algorithm>
#include <cmath>

namespace locus::app {
namespace {
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
    histogramValid_ = false;
    lastHistogramId_.clear();
    lastHistogramKey_ = {};
    lastHistoryKey_ = {};
    historyIndices_.clear();
    historyWindowValid_ = false;
    historyRowStepDp_ = 42.0f;
    if (document_) RestoreScrollDp(0.0f,1.0f);
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
    SetText("results-song", selected_ ? selected_->songTitle : chartTitle_);
    SetText("results-chart", selected_ ? selected_->chartTitle : chartTitle_);
    SetText("results-date", selected_
        ? FormatRunTimestamp(selected_->completedAtMs) : "");
    SetText("results-accuracy", Percent(selected_ ?
        selected_->accuracyPercent : grades_->AccuracyPercent()));
    SetText("results-perfect", std::to_string(grades_->perfect));
    SetText("results-good", std::to_string(grades_->good));
    SetText("results-bad", std::to_string(grades_->bad));
    SetText("results-miss", std::to_string(grades_->miss));
    // No inferred combo/zero timing for historical records without telemetry.
    const auto* statistics = selected_ && selected_->statistics
        ? &*selected_->statistics : nullptr;
    SetText("results-max-combo", statistics
        ? std::to_string(statistics->maxCombo) + "x" : "--");
    const auto timing = [](double value, bool sign, const char* unit) {
        std::ostringstream out;
        if (sign) out << std::showpos;
        out << std::fixed << std::setprecision(1) << value << unit;
        return out.str();
    };
    SetText("results-mean-error", statistics && statistics->timedHits > 0
        ? timing(statistics->meanErrorMs, true, " ms") : "--");
    SetText("results-unstable-rate", statistics && statistics->timedHits > 1
        ? timing(statistics->unstableRate, false, "") : "--");
    std::string detail;
    if (selected_ && selected_->variablePracticeRate) {
        detail = "VARIABLE RATE / LAST " + std::to_string(static_cast<int>(
            selected_->practiceRate * 100 + 0.5)) + "%";
    } else if (selected_ && selected_->practiceRate < 0.999) {
        detail = "PRACTICE " + std::to_string(static_cast<int>(
            selected_->practiceRate * 100 + 0.5)) + "%";
    }
    if (selected_) {
        const auto revision = CompareRunRevision(
            *selected_, history_->resultGameplayRevision);
        if (revision == RunRevisionState::OlderChart ||
            revision == RunRevisionState::OlderRules) {
            if (!detail.empty()) detail += " / ";
            detail += revision == RunRevisionState::OlderChart
                ? "OLDER CHART REVISION" : "OLDER JUDGEMENT RULES";
        }
    }
    SetText("results-practice", detail);
    SetText("results-replay-status",history_->replayNotice);
    if (auto* error=document_->GetElementById("results-replay-status"))
        error->SetProperty("display",history_->replayNotice.empty() ? "none" : "block");
    if (auto* button=document_->GetElementById("results-watch-replay"))
        button->SetProperty("display",selected_ && selected_->hasReplay
            ? "inline-block" : "none");
    for (const char* id : {"results-retry", "results-edit"}) {
        if (auto* button = document_->GetElementById(id))
            button->SetProperty("display", historical_ ? "none" : "inline-block");
    }
    SyncHistogram();
    SyncHistory();
}

void NativeResultsUi::SyncHistogram() {
    auto* graph = document_->GetElementById("results-hit-histogram");
    if (!graph) return;
    const std::string id = selected_ ? selected_->id : "";
    const RunKey key = selected_ ? RunKey{selected_->songSetId,selected_->difficultyId}
                                 : RunKey{};
    if (histogramValid_ && id==lastHistogramId_ && key==lastHistogramKey_) return;
    histogramValid_=true;
    lastHistogramId_=id;
    lastHistogramKey_=key;
    const auto* bins = selected_ && selected_->statistics &&
        selected_->statistics->histogram ? &*selected_->statistics->histogram : nullptr;
    // Old scoring rules may use a different window. Do not give those bins
    // current timing labels until a matching renderer is defined.
    if (!bins || selected_->judgementVersion!=core::kJudgementRulesVersion ||
        selected_->statistics->timedHits==0) {
        graph->SetInnerRML("<div class='results-muted'>NO TIMING DISTRIBUTION</div>");
        SetText("results-histogram-axis","");
        return;
    }
    const int peak=*std::max_element(bins->begin(),bins->end());
    std::ostringstream html;
    for (std::size_t i=0;i<bins->size();++i) {
        const double seconds=core::kBadWindowSeconds*
            (2.0*(static_cast<double>(i)+0.5)/bins->size()-1.0);
        const double magnitude=std::abs(seconds);
        const char* color=magnitude<=core::kPerfectWindowSeconds ? "#8ee2cc"
            : magnitude<=core::kGoodWindowSeconds ? "#9bcdf5" : "#edc68f";
        const int height=peak>0 ? ((*bins)[i]*62+peak/2)/peak : 0;
        html << "<div class='results-histogram-bin' style='height:"
             << std::max(1,height) << "dp;background-color:" << color
             << ";opacity:" << ((*bins)[i]>0 ? "1" : "0.18") << "'></div>";
    }
    graph->SetInnerRML(html.str());
    std::ostringstream axis;
    axis << "-" << core::kBadWindowSeconds*1000.0
         << " ms   /   0   /   +" << core::kBadWindowSeconds*1000.0 << " ms";
    SetText("results-histogram-axis",axis.str());
}

float NativeResultsUi::ReadScrollDp(float density) const {
    if (document_)
        if (auto* scroll = document_->GetElementById("results-history"))
            return LibraryScrollDp(scroll->GetScrollTop(),density);
    return 0.0f;
}

void NativeResultsUi::RestoreScrollDp(float position,float density) {
    if (document_)
        if (auto* scroll = document_->GetElementById("results-history"))
            scroll->SetScrollTop(LibraryScrollPixels(position,density));
}

void NativeResultsUi::SyncHistory() {
    auto* scroll = document_->GetElementById("results-history");
    auto* list = document_->GetElementById("results-history-list");
    if (!scroll || !list || !document_->GetContext()) return;
    const float density = document_->GetContext()->GetDensityIndependentPixelRatio();
    const std::string selectedId = selected_ ? selected_->id : "";
    const RunKey selectedKey = selected_
        ? RunKey{selected_->songSetId,selected_->difficultyId} : RunKey{};
    const bool newChart = selectedKey != lastHistoryKey_;
    if (newChart || lastHistoryRevision_ != history_->resultsRevision ||
        lastSelectedId_ != selectedId) {
        lastHistoryRevision_ = history_->resultsRevision;
        lastSelectedId_ = selectedId;
        lastHistoryKey_ = selectedKey;
        historyIndices_.clear();
        // Only indices are retained; immutable RunResult data stays in the
        // published history. Filter once on revision or selected attempt.
        if (selected_ && !selectedKey.Empty() && selectedKey == history_->resultKey) {
            historyIndices_.reserve(history_->results.size());
            for (std::size_t i = 0; i < history_->results.size(); ++i)
                if (history_->results[i].id != selectedId) historyIndices_.push_back(i);
        }
        historyWindowValid_ = false;
        if (newChart) RestoreScrollDp(0.0f,density);
    }

    constexpr float gap = 6.0f; // matches .results-history-list
    // Read the actual border-box height after layout, in the same dp space
    // as the spacer geometry. No scan of the complete history per frame.
    for (int i = 0; i < list->GetNumChildren(); ++i) {
        auto* row = list->GetChild(i);
        if (row && row->IsClassSet("results-history-row") && row->GetOffsetHeight() > 0.0f) {
            const float measured = LibraryScrollDp(row->GetOffsetHeight(),density) + gap;
            if (std::abs(measured-historyRowStepDp_) > 0.5f) {
                historyRowStepDp_ = measured;
                historyWindowValid_ = false;
            }
            break;
        }
    }
    const float top = ReadScrollDp(density);
    const auto window = VisibleResultRows(historyIndices_.size(),top,historyRowStepDp_,
        VirtualRowsForViewport(LibraryScrollDp(scroll->GetClientHeight(),density),
                               historyRowStepDp_),5,gap);
    if (historyWindowValid_ && historyWindow_.first == window.first &&
        historyWindow_.end == window.end) return;
    std::ostringstream html;
    const auto spacer = [&](float height) {
        if (height > 0.0f)
            html << "<div class='results-history-spacer' style='height:" << height
                 << "dp;flex:0 0 " << height << "dp'></div>";
    };
    spacer(window.before);
    for (std::size_t row = window.first; row < window.end; ++row) {
        const auto index = historyIndices_[row];
        const auto& run = history_->results[index];
        const auto revision = CompareRunRevision(run,history_->resultGameplayRevision);
        html << "<button class='results-history-row' data-action='history'"
             << " data-index='" << index << "' data-run='" << EscapeRml(run.id)
             << "'><span class='button-label'>"
             << EscapeRml(Percent(run.accuracyPercent)) << " / "
             << (revision == RunRevisionState::OlderChart ? "OLD CHART / " :
                 revision == RunRevisionState::OlderRules ? "OLD RULES / " : "")
             << EscapeRml(FormatRunTimestamp(run.completedAtMs))
             << "</span></button>";
    }
    spacer(window.after);
    if (historyIndices_.empty())
        html << "<div class='results-empty'>NO OTHER SAVED RESULTS</div>";
    // Retire focus before destroying a recycled row; never activate a
    // detached node from the previous viewport.
    for (auto* focus = document_->GetContext()->GetFocusElement(); focus;
         focus = focus->GetParentNode()) {
        if (focus == list) { document_->Focus(); break; }
    }
    list->SetInnerRML(html.str());
    RestoreScrollDp(top,density);
    historyWindow_ = window;
    historyWindowValid_ = true;
}
void NativeResultsUi::ProcessEvent(Rml::Event& event) {
    if (!document_ || !input_ || !events_ || event.GetType() != "click") return;
    auto* target = event.GetTargetElement();
    while (target && !target->HasAttribute("data-action"))
        target = target->GetParentNode();
    if (!target || target->HasAttribute("disabled")) return;
    const std::string action = target->GetAttribute<Rml::String>("data-action", "");
    if (action == "watch-replay" && selected_ && selected_->hasReplay)
        events_->watchReplay = true;
    else if (!historical_ && action == "retry") input_->startPressed = true;
    else if (!historical_ && action == "edit") input_->openEditorPressed = true;
    else if (action == "history") {
        const int index = target->GetAttribute<int>("data-index", -1);
        if (history_ && selected_ && index >= 0 &&
            index < static_cast<int>(history_->results.size())) {
            const auto& run = history_->results[static_cast<std::size_t>(index)];
            if (run.id != selected_->id &&
                run.songSetId == selected_->songSetId &&
                run.difficultyId == selected_->difficultyId &&
                run.id == target->GetAttribute<Rml::String>("data-run",""))
                events_->viewResultIndex = index;
        }
    }
}
} // namespace locus::app
