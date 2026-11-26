#include "app/native_play_ui.h"
#include "app/app_input.h"
#include "app/menu_ui.h"
#include "app/rml_ui_text.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace locus::app {
namespace {


std::string Rating(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    return stream.str();
}

std::string Accuracy(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << "%";
    return stream.str();
}

// Tree rows have two heights (42/45dp plus a 6dp gap). Use the
// shortest step to guarantee coverage even if every visible row is a chart.
LibraryRowWindow TreeViewport(const std::vector<float>& offsets,
                              Rml::Element* scroll,float density) {
    const float height = scroll ? LibraryScrollDp(scroll->GetClientHeight(),density) : 0.0f;
    const float top = scroll ? LibraryScrollDp(scroll->GetScrollTop(),density) : 0.0f;
    return VisibleLibraryRows(offsets,top,VirtualRowsForViewport(height,48.0f,6),6);
}
LibraryRowWindow ResultViewport(std::size_t count,Rml::Element* scroll,
                                float density,float step) {
    const float height = scroll ? LibraryScrollDp(scroll->GetClientHeight(),density) : 0.0f;
    const float top = scroll ? LibraryScrollDp(scroll->GetScrollTop(),density) : 0.0f;
    return VisibleResultRows(count,top,step,VirtualRowsForViewport(height,step),5);
}

} // namespace

void NativePlayUi::Attach(Rml::ElementDocument* document) noexcept {
    document_ = document;
    currentUi_ = nullptr;
    currentLibrary_ = nullptr;
    pending_ = {};
    lastDensity_ = 1.0f;
    lastTreeCache_ = nullptr;
    lastTreeGeneration_ = 0;
    lastSummarySignature_.clear();
    lastResultSignature_.clear();
    treeOffsets_.clear();
    treeFirst_ = treeEnd_ = static_cast<std::size_t>(-1);
    resultFirst_ = resultEnd_ = static_cast<std::size_t>(-1);
    resultRowStep_ = 43.0f;
    lastSelectedSong_ = lastSelectedChart_ = -1;
    lastTreeChartFocused_ = false;
}

void NativePlayUi::SetSearch(const std::string& value) {
    if (!document_) return;
    if (auto* search = dynamic_cast<Rml::ElementFormControlInput*>(document_->GetElementById("search")))
        search->SetValue(value);
}

NativePlayUi::ScrollPosition NativePlayUi::ReadScrollPosition(float density) const {
    ScrollPosition position;
    if (!document_) return position;
    if (auto* tree = document_->GetElementById("tree-scroll"))
        position.treeDp = LibraryScrollDp(tree->GetScrollTop(),density);
    if (auto* history = document_->GetElementById("results-scroll"))
        position.historyDp = LibraryScrollDp(history->GetScrollTop(),density);
    return position;
}

void NativePlayUi::RestoreScrollPosition(ScrollPosition position, float density) {
    if (!document_) return;
    if (auto* tree = document_->GetElementById("tree-scroll"))
        tree->SetScrollTop(LibraryScrollPixels(position.treeDp,density));
    if (auto* history = document_->GetElementById("results-scroll"))
        history->SetScrollTop(LibraryScrollPixels(position.historyDp,density));
}

void NativePlayUi::BeginFrame(const SongLibrary& library, SongSelectUi& ui, float density) noexcept {
    currentLibrary_ = &library;
    currentUi_ = &ui;
    lastDensity_ = density;
    pending_ = {};
}

void NativePlayUi::Sync() {
    if (document_ && currentUi_ && currentLibrary_)
        Rebuild(*currentLibrary_,*currentUi_);
}

MenuEvents NativePlayUi::EndFrame() noexcept {
    currentUi_ = nullptr;
    currentLibrary_ = nullptr;
    const auto result = pending_;
    pending_ = {};
    return result;
}

void NativePlayUi::SetText(const char* id, const std::string& value) {
    if (document_)
        UpdateRmlText(document_->GetElementById(id), value);
}

bool NativePlayUi::IsSearchFocused() const {
    if (!document_ || !document_->GetContext()) return false;
    return document_->GetContext()->GetFocusElement() == document_->GetElementById("search");
}

void NativePlayUi::ProcessEvent(Rml::Event& event) {
    auto* target = event.GetTargetElement();
    if (!target || !document_ || !currentUi_ || !currentLibrary_)
        return;
    // RmlUi's WidgetTextInput emits Change for every edit (including
    // paste/backspace), not the HTML-style Input event.
    if (event.GetType() == "change") {
        if (target->GetId() != "search") return;
        if (auto* search = dynamic_cast<Rml::ElementFormControlInput*>(target)) {
            const auto value = search->GetValue();
            if (value == currentUi_->search) return;
            currentUi_->search = value;
            currentUi_->treeScroll = 0;
            if (auto* scroll = document_->GetElementById("tree-scroll"))
                scroll->SetScrollTop(0);
            // The host's post-input Sync processes a whole burst of text
            // events once, rather than rebuilding the catalog per character.
        }
        return;
    }
    if (event.GetType() != "click") return;
    while (target && !target->HasAttribute("data-action"))
        target = target->GetParentNode();
    if (!target || target->HasAttribute("disabled")) return;
    const std::string action =
        target->GetAttribute<Rml::String>("data-action", "");
    if (action == "play") pending_.playSelected = true;
    else if (action == "mod-ht") currentUi_->mods.Toggle(core::RateMod::HalfTime);
    else if (action == "mod-dt") currentUi_->mods.Toggle(core::RateMod::DoubleTime);
    else if (action == "mod-minus") currentUi_->mods.Adjust(-1);
    else if (action == "mod-plus") currentUi_->mods.Adjust(1);
    else if (action == "edit") pending_.editSelected = true;
    else if (action == "rescan") pending_.refresh = true;
    else if (action == "import") pending_.import = true;
    else if (action == "song") {
        const int song = target->GetAttribute<int>("data-song", -1);
        if (song >= 0) {
            currentUi_->SelectSong(song, *currentLibrary_);
            currentUi_->treeChartFocused = false;
        }
    } else if (action == "chart") {
        const int song = target->GetAttribute<int>("data-song", -1);
        const int chart = target->GetAttribute<int>("data-chart", -1);
        if (song >= 0 && chart >= 0) {
            if (currentUi_->selectedSong != song)
                currentUi_->SelectSong(song, *currentLibrary_);
            currentUi_->SelectChart(chart, *currentLibrary_);
            currentUi_->treeChartFocused = true;
        }
    } else if (action == "result") {
        const int index=target->GetAttribute<int>("data-index",-1);
        if (index>=0 && index<static_cast<int>(currentUi_->results.size()) &&
            currentUi_->results[static_cast<std::size_t>(index)].id==
                target->GetAttribute<Rml::String>("data-run",""))
            pending_.viewResultIndex=index;
    }
}

void NativePlayUi::UpdateSummary(const SongLibrary& library, SongSelectUi& ui) {
    SetText("mod-ht-label", "HT " + std::to_string(ui.mods.halfTimePercent) + "%");
    SetText("mod-dt-label", "DT " + std::to_string(ui.mods.doubleTimePercent) + "%");
    for (const auto& item : {std::pair{"mod-ht",core::RateMod::HalfTime},
                             std::pair{"mod-dt",core::RateMod::DoubleTime}})
        if (auto* button = document_->GetElementById(item.first))
            button->SetClass("selected", ui.mods.rateMod == item.second);
    const int percent = ui.mods.RatePercent();
    const bool active = ui.mods.rateMod != core::RateMod::None;
    const int minimum = ui.mods.rateMod == core::RateMod::HalfTime ? 50 : 101;
    const int maximum = ui.mods.rateMod == core::RateMod::HalfTime ? 99 : 150;
    if (auto* button = document_->GetElementById("mod-minus")) {
        if (active && percent > minimum) button->RemoveAttribute("disabled");
        else button->SetAttribute("disabled", "");
    }
    if (auto* button = document_->GetElementById("mod-plus")) {
        if (active && percent < maximum) button->RemoveAttribute("disabled");
        else button->SetAttribute("disabled", "");
    }
    const LibrarySong* song = ui.selectedSong > 0 &&
        ui.selectedSong <= static_cast<int>(library.Songs().size())
        ? &library.Songs()[static_cast<std::size_t>(ui.selectedSong - 1)]
        : nullptr;
    const LibraryChart* chart = ui.Selected(library);
    const auto rating = chart ? chart->difficulty :
        core::CalculateChartDifficulty(core::MakeDemoChart());
    // Initial library-index telemetry is not a user-facing notification.
    SetText("notice", ui.notice.rfind("LIBRARY:", 0) == 0
        ? "" : ui.notice);
    SetText("song-title", song ? song->title : "BUILT-IN DEMO");
    SetText("song-author", song
        ? (song->artist.empty() ? "UNKNOWN ARTIST" : song->artist)
        : "LOCUS");
    SetText("song-mapper", song && !song->mapper.empty()
        ? "Mapped by " + song->mapper : "");
    SetText("song-meta", song
        ? Rating(song->bpm) + " BPM  /  " +
          std::to_string(song->charts.size()) + " CHARTS"
        : "120 BPM  /  1 CHART");
    std::string tags;
    if (song) {
        for (std::size_t i = 0; i < song->tags.size() && i < 3; ++i) {
            if (!tags.empty()) tags += "  /  ";
            tags += song->tags[i];
        }
    }
    SetText("song-tags", tags);
    SetText("chart-title", chart ? chart->metadata.title : "ORIGINAL");
    SetText("rating", Rating(rating.overall));
    SetText("chart-notes", std::to_string(rating.notes) + " EVENTS / " +
        std::to_string(rating.keyPresses) + " KEYS");
    SetText("metric-speed", Rating(rating.speed));
    SetText("metric-rhythm", Rating(rating.rhythm));
    SetText("metric-coord", Rating(rating.coordination));
    SetText("metric-reading", Rating(rating.reading));
    SetText("metric-stamina", Rating(rating.stamina));
    SetText("metric-fingers", Rating(rating.fingerStrain));

}

void NativePlayUi::RebuildRows(const SongLibrary& library, SongSelectUi& ui) {
    auto* rows = document_->GetElementById("tree");
    if (!rows) return;
    auto* scroll = document_->GetElementById("tree-scroll");
    const float oldScroll = scroll ? scroll->GetScrollTop() : 0;
    const auto view = TreeViewport(treeOffsets_,scroll,lastDensity_);
    const auto& entries = LibraryTreeRows(library,ui);
    std::ostringstream html;
    if (view.before > 0.0f)
        html << "<div class='tree-spacer' style='height:"
             << view.before << "dp;flex:0 0 " << view.before << "dp'></div>";
    for (std::size_t index = view.first; index < view.end; ++index) {
        const auto& item = entries[index];
        const bool isChart = item.chart >= 0;
        const bool active = item.song == ui.selectedSong &&
            (isChart ? ui.treeChartFocused && item.chart == ui.selectedChart
                     : !ui.treeChartFocused);
        std::string name;
        double difficulty = 0;
        if (item.song == 0) {
            name = isChart ? "ORIGINAL" : "BUILT-IN DEMO";
            difficulty = core::CalculateChartDifficulty(
                core::MakeDemoChart()).overall;
        } else {
            const auto& song = library.Songs()[
                static_cast<std::size_t>(item.song - 1)];
            if (!isChart) name = song.title;
            else {
                const auto& chart = song.charts[
                    static_cast<std::size_t>(item.chart)];
                name = chart.metadata.title;
                difficulty = chart.difficulty.overall;
            }
        }
        html << "<button class='" << (isChart ? "chart-row" : "song-row")
             << (active ? " active" : "")
             << "' data-action='" << (isChart ? "chart" : "song")
             << "' data-song='" << item.song << "'";
        if (isChart) html << " data-chart='" << item.chart << "'";
        html << ">";
        if (isChart) html << "<span class='chart-marker'></span>";
        else html << "<span class='row-chevron"
                  << (item.song == ui.selectedSong ? " expanded" : "")
                  << "'></span>";
        html << "<span class='row-label'>" << EscapeRml(name) << "</span>";
        if (isChart) html << "<span class='row-rating'>"
                          << Rating(difficulty) << "</span>";
        else {
            const std::size_t count = item.song == 0 ? 1 :
                library.Songs()[static_cast<std::size_t>(item.song - 1)]
                    .charts.size();
            html << "<span class='row-count'>" << count << "</span>";
        }
        html << "</button>";
    }
    if (view.after > 0.0f)
        html << "<div class='tree-spacer' style='height:"
             << view.after << "dp;flex:0 0 " << view.after << "dp'></div>";
    if (entries.empty()) html << "<div class='empty'>NO MATCHING SONGS</div>";
    for (auto* focus=document_->GetContext()->GetFocusElement(); focus;
         focus=focus->GetParentNode()) {
        if (focus==rows) { document_->Focus(); break; }
    }
    rows->SetInnerRML(html.str());
    if (scroll) scroll->SetScrollTop(oldScroll);
    treeFirst_ = view.first;
    treeEnd_ = view.end;
    SetText("set-count", std::to_string(library.Songs().size() + 1));
}

void NativePlayUi::SyncTreeSelection(const SongSelectUi& ui) {
    auto* rows = document_->GetElementById("tree");
    if (!rows) return;
    for (int i=0; i<rows->GetNumChildren(); ++i) {
        auto* row=rows->GetChild(i);
        if (!row || !row->HasAttribute("data-song")) continue;
        const bool active = row->GetAttribute<int>("data-song",-1)==ui.selectedSong &&
            row->GetAttribute<int>("data-chart",-1)==
                (ui.treeChartFocused ? ui.selectedChart : -1);
        if (row->IsClassSet("active") != active) row->SetClass("active",active);
    }
}

void NativePlayUi::RebuildResults(SongSelectUi& ui) {
    auto* rows = document_->GetElementById("results-list");
    if (!rows) return;
    auto* scroll = document_->GetElementById("results-scroll");
    const float oldScroll = scroll ? scroll->GetScrollTop() : 0;
    SetText("result-count", std::to_string(ui.results.size()));
    const auto window = ResultViewport(ui.results.size(),scroll,lastDensity_,resultRowStep_);
    std::ostringstream html;
    if (window.before > 0.0f)
        html << "<div class='result-spacer' style='height:"
             << window.before << "dp;flex:0 0 " << window.before << "dp'></div>";
    for (std::size_t i = window.first; i < window.end; ++i) {
        const auto& result = ui.results[i];
        const auto rank=core::RankForAccuracy(result.accuracyPercent,result.grades.Resolved());
        const char* name=core::ResultRankName(rank);
        html << "<button class='result' data-action='result' data-index='"
             << i << "' data-run='" << EscapeRml(result.id)
             << "'><span class='button-label result-rank rank-"
             << (rank==core::ResultRank::None ? "none" : name)
             << "'>" << name << "</span><span class='button-label result-accuracy'>"
             << Accuracy(result.accuracyPercent)
             << "</span><span class='button-label result-combo'>"
             << FormatRunCombo(result) << "</span><span class='button-label result-mod'>"
             << (result.variablePracticeRate ? "VAR" : result.practiceRate == 1.0 ? "NM" :
                 core::FormatRateMod(static_cast<int>(std::lround(result.practiceRate*100))))
             << "</span></button>";
    }
    if (window.after > 0.0f)
        html << "<div class='result-spacer' style='height:"
             << window.after << "dp;flex:0 0 " << window.after << "dp'></div>";
    if (ui.results.empty())
        html << "<div class='empty'>"
             << (ui.resultsLoading ? "LOADING RESULTS..." : "NO SAVED RESULTS")
             << "</div>";
    rows->SetInnerRML(html.str());
    if (scroll) scroll->SetScrollTop(oldScroll);
    resultFirst_ = window.first;
    resultEnd_ = window.end;
}

void NativePlayUi::Rebuild(const SongLibrary& library, SongSelectUi& ui) {
    // Share the cached structural projection with keyboard and fallback.
    // Moving between children only changes the active class and scroll.
    const auto& entries = LibraryTreeRows(library,ui);
    const bool structureChanged = lastTreeCache_ != &ui.treeCache ||
        lastTreeGeneration_ != ui.treeCache.generation;
    const bool selectionChanged = ui.selectedSong != lastSelectedSong_ ||
        ui.selectedChart != lastSelectedChart_ ||
        ui.treeChartFocused != lastTreeChartFocused_;
    if (structureChanged) {
        treeOffsets_ = LibraryRowOffsets(entries);
        lastTreeCache_ = &ui.treeCache;
        lastTreeGeneration_ = ui.treeCache.generation;
        treeFirst_ = treeEnd_ = static_cast<std::size_t>(-1);
    }
    auto* scroll = document_->GetElementById("tree-scroll");
    if (selectionChanged && scroll && scroll->GetClientHeight() > 0.0f) {
        const auto index = FindLibraryTreeRow(entries,ui.selectedSong,
            ui.treeChartFocused ? ui.selectedChart : -1);
        if (index < entries.size()) {
            const float start = treeOffsets_[index];
            const float bottom = treeOffsets_[index+1]-6.0f;
            const float viewportStart = LibraryScrollDp(scroll->GetScrollTop(),lastDensity_);
            const float height = LibraryScrollDp(scroll->GetClientHeight(),lastDensity_);
            if (start < viewportStart)
                scroll->SetScrollTop(LibraryScrollPixels(start,lastDensity_));
            else if (bottom > viewportStart+height)
                scroll->SetScrollTop(LibraryScrollPixels(bottom-height,lastDensity_));
        }
    }
    const auto window = TreeViewport(treeOffsets_,scroll,lastDensity_);
    if (window.first != treeFirst_ || window.end != treeEnd_)
        RebuildRows(library,ui);
    else if (selectionChanged)
        SyncTreeSelection(ui);
    lastSelectedSong_ = ui.selectedSong;
    lastSelectedChart_ = ui.selectedChart;
    lastTreeChartFocused_ = ui.treeChartFocused;
    // Status text may change without a selection/index change. Updating it
    // must never recreate all rows or drop scroll position/focus.
    std::ostringstream summary;
    summary << library.Revision() << ':' << ui.selectedSong << ':'
            << ui.selectedChart << ':' << ui.notice << ':'
            << static_cast<int>(ui.mods.rateMod) << ':' << ui.mods.halfTimePercent
            << ':' << ui.mods.doubleTimePercent;
    if (summary.str() != lastSummarySignature_) {
        UpdateSummary(library, ui);
        lastSummarySignature_ = summary.str();
    }
    const std::string history = std::to_string(ui.resultsRevision);
    if (history != lastResultSignature_) {
        resultFirst_ = resultEnd_ = static_cast<std::size_t>(-1);
        resultRowStep_ = 43.0f;
        lastResultSignature_ = history;
        // Selecting a different difficulty is a new history, not another
        // window of the previous scroll position.
        if (auto* scroll = document_->GetElementById("results-scroll"))
            scroll->SetScrollTop(0);
    }
    // Infer the real height from RmlUi layout after the previous frame.
    // Padding/borders can change at high DPI; a fixed CSS assumption would
    // make virtual spacers shift when scrolling thousands of attempts.
    if (auto* list = document_->GetElementById("results-list")) {
        for (int i = 0; i < list->GetNumChildren(); ++i) {
            auto* child = list->GetChild(i);
            if (child && child->IsClassSet("result") &&
                child->GetOffsetHeight() > 0.0f) {
                const float step = LibraryResultRowStepDp(child->GetOffsetHeight(),lastDensity_);
                if (std::abs(step - resultRowStep_) > 0.5f) {
                    resultRowStep_ = step;
                    resultFirst_ = resultEnd_ = static_cast<std::size_t>(-1);
                }
                break;
            }
        }
    }
    auto* historyScroll = document_->GetElementById("results-scroll");
    const auto visible = ResultViewport(ui.results.size(),historyScroll,lastDensity_,resultRowStep_);
    if (visible.first != resultFirst_ || visible.end != resultEnd_)
        RebuildResults(ui);
}

void NativePlayUi::ProcessKeyboard(FrameInput& input) {
    if (!document_ || !currentUi_ || !currentLibrary_) return;
    auto* context = document_->GetContext();
    if (!context) return;
    // Resolve ownership before activation, because a click may rebuild the
    // focused subtree. A single Enter must never fall through to PLAY.
    if (pending_.viewResultIndex>=0) {
        SuppressGlobalMenuShortcuts(input);
        return;
    }
    if (IsSearchFocused()) {
        if (input.cancelLibrarySearch || input.startPressed)
            if (auto* search = document_->GetElementById("search")) search->Blur();
        SuppressGlobalMenuShortcuts(input);
        return;
    }
    if (auto* focus = context->GetFocusElement();
        input.startPressed && focus && focus->GetTagName() == "button") {
        const auto action = focus->GetAttribute<Rml::String>("data-action", "");
        // Song/chart rows are selection, not action buttons. Once selected,
        // Enter starts the active difficulty (also after keyboard navigation).
        // An explicit action button, such as Edit, still owns its activation.
        if (action != "song" && action != "chart") {
            if (!pending_.playSelected && !pending_.editSelected &&
                pending_.viewResultIndex < 0 && !pending_.import && !pending_.refresh &&
                !focus->HasAttribute("disabled")) focus->Click();
            SuppressGlobalMenuShortcuts(input);
            return;
        }
    }
    auto& ui = *currentUi_;
    const auto& library = *currentLibrary_;
    if (input.libraryTreeDelta) ui.MoveTree(input.libraryTreeDelta,library);
    if (input.libraryChartDelta) ui.MoveChart(input.libraryChartDelta,library);

    // A pointer command already emitted by RmlUi owns this frame. Otherwise
    // resolve the same configured shortcuts as the fallback browser.
    if (!pending_.playSelected && !pending_.editSelected &&
        pending_.viewResultIndex < 0 && !pending_.import && !pending_.refresh) {
        if (input.openEditorPressed) pending_.editSelected = true;
        else if (input.startPressed) pending_.playSelected = true;
        else if (input.refreshLibraryPressed) pending_.refresh = true;
    }
    input.startPressed = false;
    input.openEditorPressed = false;
    input.refreshLibraryPressed = false;
}

} // namespace locus::app
