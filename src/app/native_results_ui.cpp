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
    rankValid_=false;
    histogramValid_ = false;
    lastHistogramId_.clear();
    lastHistogramKey_ = {};
    if (document_) RestoreScrollDp(0.0f,1.0f);
}
void NativeResultsUi::BeginFrame(
    const RunResult* selected, const core::GradeCounts& grades,
    int /*total*/, const std::string& title, bool historical,
    const SongSelectUi& history, FrameInput& input,
    MenuEvents& events) noexcept {
    selected_ = selected;
    grades_ = &grades;
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
    const double accuracy=selected_ ? selected_->accuracyPercent : grades_->AccuracyPercent();
    SetText("results-accuracy", Percent(accuracy));
    const auto rank=core::RankForAccuracy(accuracy,grades_->Resolved());
    if (!rankValid_ || rank!=lastRank_) {
        SetText("results-rank",core::ResultRankName(rank));
        if (auto* element=document_->GetElementById("results-rank"))
            element->SetClassNames(std::string("results-rank rank-")+
                (rank==core::ResultRank::None ? "none" : core::ResultRankName(rank)));
        lastRank_=rank;
        rankValid_=true;
    }
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
    } else if (selected_) {
        detail = FormatRunMods(*selected_);
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
    const auto pp=selected_?RunPerformance(*selected_):std::nullopt;
    SetText("results-performance",pp ? timing(*pp,false," PP (V1 / LOCAL)") : "");
    SetText("results-replay-status",history_->replayNotice);
    if (auto* error=document_->GetElementById("results-replay-status"))
        error->SetProperty("display",history_->replayNotice.empty() ? "none" : "block");
    if (auto* button=document_->GetElementById("results-watch-replay"))
        button->SetProperty("display",selected_ && selected_->hasReplay
            ? "inline-block" : "none");
    if (auto* button=document_->GetElementById("results-retry"))
        button->SetProperty("display",historical_ ? "none" : "inline-block");
    SyncHistogram();
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
    const double limitMs = core::kBadWindowSeconds*core::WindowScale(selected_->modFlags)*1000.0;
    axis << "-" << limitMs << " ms   /   0   /   +" << limitMs << " ms";
    SetText("results-histogram-axis",axis.str());
}

float NativeResultsUi::ReadScrollDp(float density) const {
    if (document_)
        if (auto* scroll = document_->GetElementById("results-content"))
            return LibraryScrollDp(scroll->GetScrollTop(),density);
    return 0.0f;
}

void NativeResultsUi::RestoreScrollDp(float position,float density) {
    if (document_)
        if (auto* scroll = document_->GetElementById("results-content"))
            scroll->SetScrollTop(LibraryScrollPixels(position,density));
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
}
} // namespace locus::app
