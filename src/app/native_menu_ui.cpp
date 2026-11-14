#include "app/native_menu_ui.h"

#include "app/menu_ui.h"
#include "app/viewport.h"
#include "core/utf8.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StyleTypes.h>

#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace locus::app {
namespace {

std::string Escape(const std::string& source) {
    std::string out;
    out.reserve(source.size());
    for (char c : source) {
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

int Modifiers() {
    int result = 0;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
        result |= Rml::Input::KM_SHIFT;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
        result |= Rml::Input::KM_CTRL;
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))
        result |= Rml::Input::KM_ALT;
    return result;
}

Rml::Input::KeyIdentifier ToRmlKey(int raylibKey) {
    using namespace Rml::Input;
    if (raylibKey >= KEY_A && raylibKey <= KEY_Z)
        return static_cast<KeyIdentifier>(KI_A + raylibKey - KEY_A);
    if (raylibKey >= KEY_ZERO && raylibKey <= KEY_NINE)
        return static_cast<KeyIdentifier>(KI_0 + raylibKey - KEY_ZERO);
    switch (raylibKey) {
        case KEY_BACKSPACE: return KI_BACK;
        case KEY_DELETE: return KI_DELETE;
        case KEY_ENTER: return KI_RETURN;
        case KEY_KP_ENTER: return KI_NUMPADENTER;
        case KEY_TAB: return KI_TAB;
        case KEY_SPACE: return KI_SPACE;
        case KEY_ESCAPE: return KI_ESCAPE;
        case KEY_LEFT: return KI_LEFT;
        case KEY_RIGHT: return KI_RIGHT;
        case KEY_UP: return KI_UP;
        case KEY_DOWN: return KI_DOWN;
        case KEY_HOME: return KI_HOME;
        case KEY_END: return KI_END;
        case KEY_PAGE_UP: return KI_PRIOR;
        case KEY_PAGE_DOWN: return KI_NEXT;
        case KEY_LEFT_SHIFT: return KI_LSHIFT;
        case KEY_RIGHT_SHIFT: return KI_RSHIFT;
        case KEY_LEFT_CONTROL: return KI_LCONTROL;
        case KEY_RIGHT_CONTROL: return KI_RCONTROL;
        case KEY_LEFT_ALT: return KI_LMENU;
        case KEY_RIGHT_ALT: return KI_RMENU;
        default: return KI_UNKNOWN;
    }
}
constexpr int kUiKeys[] = {
    KEY_BACKSPACE, KEY_DELETE, KEY_ENTER, KEY_KP_ENTER,
    KEY_TAB, KEY_SPACE, KEY_ESCAPE,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_HOME, KEY_END,
    KEY_PAGE_UP, KEY_PAGE_DOWN,
    KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT,
    KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL,
    KEY_LEFT_ALT, KEY_RIGHT_ALT
};

bool LoadUiFontFace() {
    namespace fs = std::filesystem;
    const char* preferred[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
    };
    for (const char* path : preferred) {
        if (!fs::exists(fs::u8path(path))) continue;
        if (Rml::LoadFontFace(path, "Locus", Rml::Style::FontStyle::Normal,
                              Rml::Style::FontWeight::Normal)) {
            // The approved hierarchy requires real bold face glyphs.
            const fs::path regular = fs::u8path(path);
            const fs::path bold = regular.parent_path() /
                (regular.filename() == "segoeui.ttf" ? "segoeuib.ttf" :
                 regular.filename() == "arial.ttf" ? "arialbd.ttf" :
                 regular.filename() == "DejaVuSans.ttf" ? "DejaVuSans-Bold.ttf" :
                 "LiberationSans-Bold.ttf");
            if (fs::exists(bold))
                Rml::LoadFontFace(bold.u8string(), "Locus",
                    Rml::Style::FontStyle::Normal,
                    Rml::Style::FontWeight::Bold);
            return true;
        }
    }
    return false;
}

} // namespace

NativeMenuUi::~NativeMenuUi() { Shutdown(); }

bool NativeMenuUi::Initialize() {
    if (Available()) return true;
    if (GetScreenWidth() <= 0 || GetScreenHeight() <= 0) return false;
    Rml::SetSystemInterface(&system_);
    Rml::SetRenderInterface(&renderer_);
    if (!Rml::Initialise()) return false;
    const bool fontLoaded = LoadUiFontFace();
    if (!fontLoaded) {
        std::cerr << "RmlUi font unavailable; using legacy PLAY UI.\n";
        Rml::Shutdown();
        return false;
    }
    context_ = Rml::CreateContext("locus-menu", {
        GetScreenWidth(), GetScreenHeight()});
    if (!context_) {
        Rml::Shutdown();
        return false;
    }
    const std::filesystem::path file =
        std::filesystem::u8path(GetApplicationDirectory()) /
        "assets" / "ui" / "rml" / "play.rml";
    document_ = context_->LoadDocument(file.u8string());
    if (!document_) {
        std::cerr << "Could not load PLAY RmlUi file: " << file.u8string() << '\n';
        Rml::RemoveContext("locus-menu");
        context_ = nullptr;
        Rml::Shutdown();
        return false;
    }
    // Home is optional: a bad document must not disable working PLAY.
    homeDocument_ = context_->LoadDocument(
        (file.parent_path() / "home.rml").u8string());
    if (homeDocument_) {
        homeDocument_->AddEventListener("click", this);
        homeDocument_->Hide();
    } else {
        std::cerr << "Could not load HOME RmlUi; using legacy HOME UI.\n";
    }
    settingsDocument_ = context_->LoadDocument(
        (file.parent_path() / "settings.rml").u8string());
    if (settingsDocument_) {
        settingsDocument_->AddEventListener("click", this);
        settingsDocument_->Hide();
        settingsPresenter_.Attach(settingsDocument_);
    } else {
        std::cerr << "Could not load SETTINGS RmlUi; using legacy settings.\n";
    }
    editorDocument_ = context_->LoadDocument(
        (file.parent_path() / "editor.rml").u8string());
    if (editorDocument_) {
        editorDocument_->AddEventListener("click", this);
        editorDocument_->Hide();
        editorPresenter_.Attach(editorDocument_);
    } else {
        std::cerr << "Could not load editor chrome; using raylib editor UI.\n";
    }
    document_->AddEventListener("click", this);
    document_->AddEventListener("input", this);
    document_->Hide();
    context_->Update();
    return true;
}

void NativeMenuUi::Shutdown() {
    if (context_) {
        if (document_) {
            document_->RemoveEventListener("click", this);
            document_->RemoveEventListener("input", this);
        }
        if (homeDocument_) homeDocument_->RemoveEventListener("click", this);
        if (settingsDocument_)
            settingsDocument_->RemoveEventListener("click", this);
        settingsPresenter_.Attach(nullptr);
        settingsDocument_ = nullptr;
        if (editorDocument_)
            editorDocument_->RemoveEventListener("click", this);
        editorPresenter_.Attach(nullptr);
        editorDocument_ = nullptr;
        document_ = nullptr;
        homeDocument_ = nullptr;
        context_->UnloadAllDocuments();
        Rml::RemoveContext("locus-menu");
        context_ = nullptr;
        Rml::Shutdown();
        renderer_.ReleaseAllTextures();
    }
    visiblePage_ = Page::None;
    pendingHomeAction_ = -1;
    currentUi_ = nullptr;
    currentLibrary_ = nullptr;
}

void NativeMenuUi::Activate(Page page) {
    if (visiblePage_ == page) return;
    if (document_) document_->Hide();
    if (homeDocument_) homeDocument_->Hide();
    if (settingsDocument_) settingsDocument_->Hide();
    if (editorDocument_) editorDocument_->Hide();
    visiblePage_ = page;
    if (page == Page::Home && homeDocument_) {
        homeDocument_->Show();
        lastHomeNotice_.clear();
        if (auto* notice = homeDocument_->GetElementById("home-notice"))
            notice->SetInnerRML("");
    } else if (page == Page::Play && document_) {
        document_->Show();
        lastTreeSignature_.clear();
        lastSummarySignature_.clear();
        lastResultSignature_.clear();
    } else if (page == Page::Settings && settingsDocument_) {
        settingsDocument_->Show();
        settingsPresenter_.Attach(settingsDocument_);
    } else if (page == Page::Editor && editorDocument_) {
        editorDocument_->Show();
        editorPresenter_.Attach(editorDocument_);
        lastEditorWidth_ = -1;
        lastEditorHeight_ = -1;
    }
    ::SetMouseCursor(MOUSE_CURSOR_DEFAULT);
}

bool NativeMenuUi::ShowingHome() const noexcept {
    return visiblePage_ == Page::Home;
}

bool NativeMenuUi::ShowingPlay() const noexcept {
    return visiblePage_ == Page::Play;
}

bool NativeMenuUi::ShowingSettings() const noexcept {
    return visiblePage_ == Page::Settings;
}
bool NativeMenuUi::ShowingEditor() const noexcept {
    return visiblePage_ == Page::Editor;
}

Rectangle NativeMenuUi::HomeWordmarkBounds() const noexcept {
    if (!ShowingHome() || !homeDocument_) return {};
    if (auto* element = homeDocument_->GetElementById("home-wordmark"))
        return {element->GetAbsoluteLeft(), element->GetAbsoluteTop(),
                static_cast<float>(element->GetOffsetWidth()),
                static_cast<float>(element->GetOffsetHeight())};
    return {};
}

void NativeMenuUi::Hide() {
    if (visiblePage_ == Page::None) return;
    Activate(Page::None);
    currentUi_ = nullptr;
    currentLibrary_ = nullptr;
}

void NativeMenuUi::SetText(const char* id, const std::string& value) {
    if (auto* element = document_->GetElementById(id))
        element->SetInnerRML(Escape(value));
}

bool NativeMenuUi::IsSearchFocused() const {
    if (!context_ || !document_) return false;
    return context_->GetFocusElement() == document_->GetElementById("search");
}

void NativeMenuUi::ProcessEvent(Rml::Event& event) {
    auto* target = event.GetTargetElement();
    if (!target) return;
    if (visiblePage_ == Page::Home) {
        if (event.GetType() != "click") return;
        while (target && !target->HasAttribute("data-action"))
            target = target->GetParentNode();
        if (!target) return;
        const std::string action =
            target->GetAttribute<Rml::String>("data-action", "");
        if (action == "home-play") pendingHomeAction_ = 0;
        else if (action == "home-editor") pendingHomeAction_ = 1;
        else if (action == "home-skins") pendingHomeAction_ = 2;
        else if (action == "home-settings") pendingHomeAction_ = 3;
        else if (action == "home-exit") pendingHomeAction_ = 4;
        return;
    }
    if (visiblePage_ == Page::Settings) {
        settingsPresenter_.ProcessEvent(event);
        return;
    }
    if (visiblePage_ == Page::Editor) {
        editorPresenter_.ProcessEvent(event);
        return;
    }
    if (visiblePage_ != Page::Play || !currentUi_ || !currentLibrary_)
        return;
    if (event.GetType() == "input") {
        if (target->GetId() != "search") return;
        if (auto* search = dynamic_cast<Rml::ElementFormControlInput*>(target)) {
            currentUi_->search = search->GetValue();
            currentUi_->treeScroll = 0;
            RebuildRows(*currentLibrary_, *currentUi_);
        }
        return;
    }
    if (event.GetType() != "click") return;
    while (target && !target->HasAttribute("data-action"))
        target = target->GetParentNode();
    if (!target) return;
    const std::string action =
        target->GetAttribute<Rml::String>("data-action", "");
    if (action == "back") pending_.backRequested = true;
    else if (action == "play") pending_.playSelected = true;
    else if (action == "edit") pending_.editSelected = true;
    else if (action == "rescan") pending_.refresh = true;
    else if (action == "import") pending_.import = true;
    else if (action == "song-set") {
        pending_.editSelected = true;
        pending_.openSet = true;
    }
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
        pending_.viewResultIndex = target->GetAttribute<int>("data-index", -1);
    }
}

void NativeMenuUi::UpdateSummary(const SongLibrary& library, SongSelectUi& ui) {
    const LibrarySong* song = ui.selectedSong > 0 &&
        ui.selectedSong <= static_cast<int>(library.Songs().size())
        ? &library.Songs()[static_cast<std::size_t>(ui.selectedSong - 1)]
        : nullptr;
    const LibraryChart* chart = ui.Selected(library);
    const auto rating = chart ? chart->difficulty :
        core::CalculateChartDifficulty(core::MakeDemoChart());
    const int count = chart
        ? static_cast<int>(chart->data.route.turns.size())
        : static_cast<int>(core::MakeDemoChart().turns.size());
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
    SetText("chart-title", chart ? chart->data.title : "ORIGINAL");
    SetText("rating", Rating(rating.overall));
    SetText("chart-notes", std::to_string(count) + " TURNS");
    SetText("metric-speed", Rating(rating.speed));
    SetText("metric-rhythm", Rating(rating.rhythm));
    SetText("metric-coord", Rating(rating.coordination));
    SetText("metric-reading", Rating(rating.reading));
    SetText("metric-stamina", Rating(rating.stamina));
    SetText("metric-peak", Rating(rating.peak));
    if (auto* editSet = document_->GetElementById("set-button")) {
        if (song && song->managed && chart)
            editSet->RemoveAttribute("disabled");
        else
            editSet->SetAttribute("disabled", "");
    }
}

void NativeMenuUi::RebuildRows(const SongLibrary& library, SongSelectUi& ui) {
    auto* rows = document_->GetElementById("tree");
    if (!rows) return;
    auto* scroll = document_->GetElementById("tree-scroll");
    const float oldScroll = scroll ? scroll->GetScrollTop() : 0;
    const auto entries = LibraryTreeRows(library, ui);
    std::ostringstream html;
    for (const auto& item : entries) {
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
                name = chart.data.title;
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
        html << "<span class='row-label'>" << Escape(name) << "</span>";
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
    if (entries.empty()) html << "<div class='empty'>NO MATCHING SONGS</div>";
    rows->SetInnerRML(html.str());
    if (scroll) scroll->SetScrollTop(oldScroll);
    SetText("set-count", std::to_string(library.Songs().size() + 1));
}

void NativeMenuUi::RebuildResults(SongSelectUi& ui) {
    auto* rows = document_->GetElementById("results-list");
    if (!rows) return;
    auto* scroll = document_->GetElementById("results-scroll");
    const float oldScroll = scroll ? scroll->GetScrollTop() : 0;
    SetText("result-count", std::to_string(ui.results.size()));
    std::ostringstream html;
    for (std::size_t i = 0; i < ui.results.size(); ++i) {
        const auto& result = ui.results[i];
        html << "<button class='result' data-action='result' data-index='"
             << i << "'><strong>" << Accuracy(result.accuracyPercent)
             << "</strong><span class='result-date'>"
             << Escape(FormatRunTimestamp(result.completedAtMs))
             << "</span><span class='result-arrow'>&gt;</span></button>";
    }
    if (ui.results.empty())
        html << "<div class='empty'>NO SAVED RESULTS</div>";
    rows->SetInnerRML(html.str());
    if (scroll) scroll->SetScrollTop(oldScroll);
}

void NativeMenuUi::Rebuild(const SongLibrary& library, SongSelectUi& ui) {
    // Only changes that affect tree geometry trigger its expensive DOM rebuild.
    // Library::Revision is advanced by index refreshes and in-place metadata
    // changes, so a 10,000-chart library costs O(1) to poll on idle frames.
    std::ostringstream tree;
    tree << library.Revision() << ':' << ui.selectedSong << ':'
         << ui.selectedChart << ':' << ui.treeChartFocused << ':'
         << ui.search;
    if (tree.str() != lastTreeSignature_) {
        RebuildRows(library, ui);
        lastTreeSignature_ = tree.str();
    }
    // Status text may change without a selection/index change. Updating it
    // must never recreate all rows or drop scroll position/focus.
    std::ostringstream summary;
    summary << library.Revision() << ':' << ui.selectedSong << ':'
            << ui.selectedChart << ':' << ui.notice;
    if (summary.str() != lastSummarySignature_) {
        UpdateSummary(library, ui);
        lastSummarySignature_ = summary.str();
    }
    std::ostringstream history;
    history << ui.resultKey.songSetId << '/' << ui.resultKey.difficultyFile;
    for (const auto& result : ui.results)
        history << result.id << ':' << result.accuracyPercent;
    if (history.str() != lastResultSignature_) {
        RebuildResults(ui);
        lastResultSignature_ = history.str();
    }
}

void NativeMenuUi::PumpInput(bool dispatchKeyboard) {
    const int width = GetScreenWidth(), height = GetScreenHeight();
    if (width != lastWidth_ || height != lastHeight_) {
        context_->SetDimensions({std::max(1,width),std::max(1,height)});
        lastWidth_ = width; lastHeight_ = height;
    }
    // DPI can change when the window moves between monitors.
    // Match the 960x540 approved design rather than monitor DPI. Extra
    // ultrawide space is handled by the responsive flex container.
    const float density = visiblePage_ == Page::Editor
        ? std::max(0.05f, ViewportTransform::Menu(width, height).scale)
        : std::clamp(std::min(
            width / 960.0f, height / 540.0f), 0.7f, 3.0f);
    if (std::abs(density - lastDensity_) > 0.01f) {
        context_->SetDensityIndependentPixelRatio(density);
        lastDensity_ = density;
    }
    const int mods = Modifiers();
    const auto mouse = GetMousePosition();
    context_->ProcessMouseMove(static_cast<int>(mouse.x),
                               static_cast<int>(mouse.y), mods);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        context_->ProcessMouseButtonDown(0, mods);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        context_->ProcessMouseButtonUp(0, mods);
    const float wheel = GetMouseWheelMove();
    if (wheel != 0) context_->ProcessMouseWheel(-wheel, mods);
    // Exclusive key capture, manual offset and calibration must never leak
    // their keyboard events into RmlUi buttons or navigation.
    if (!dispatchKeyboard) return;
    for (int key = KEY_A; key <= KEY_Z; ++key) {
        const auto name = ToRmlKey(key);
        if (IsKeyPressed(key)) context_->ProcessKeyDown(name, mods);
        if (IsKeyReleased(key)) context_->ProcessKeyUp(name, mods);
    }
    for (int key = KEY_ZERO; key <= KEY_NINE; ++key) {
        const auto name = ToRmlKey(key);
        if (IsKeyPressed(key)) context_->ProcessKeyDown(name, mods);
        if (IsKeyReleased(key)) context_->ProcessKeyUp(name, mods);
    }
    for (const int key : kUiKeys) {
        const auto name = ToRmlKey(key);
        if (IsKeyPressed(key)) context_->ProcessKeyDown(name, mods);
        if (IsKeyReleased(key)) context_->ProcessKeyUp(name, mods);
    }
    int codepoint = 0;
    while ((codepoint = GetCharPressed()) != 0)
        context_->ProcessTextInput(static_cast<Rml::Character>(codepoint));
}

void NativeMenuUi::UpdateEditor(EditorUiState& ui, FrameInput& input,
                                const std::string& title, bool dirty,
                                double rate, bool playing,
                                long long songTimeMs) {
    if (!EditorAvailable()) return;
    Activate(Page::Editor);
    currentLibrary_ = nullptr;
    currentUi_ = nullptr;
    const auto viewport = ViewportTransform::Menu(
        GetScreenWidth(), GetScreenHeight());
    // The editor's physical letterbox is shared with the raylib viewport,
    // without inheriting any legacy button hitboxes.
    if (lastEditorWidth_ != GetScreenWidth() ||
        lastEditorHeight_ != GetScreenHeight()) {
        if (auto* canvas = editorDocument_->GetElementById("editor-canvas")) {
            const float scale = std::max(0.05f, viewport.scale);
            canvas->SetProperty("left", std::to_string(viewport.offsetX / scale) + "dp");
            canvas->SetProperty("top", std::to_string(viewport.offsetY / scale) + "dp");
        }
        lastEditorWidth_ = GetScreenWidth();
        lastEditorHeight_ = GetScreenHeight();
    }
    editorPresenter_.BeginFrame(ui, input);
    editorPresenter_.Sync(ui, title, dirty, rate, playing, songTimeMs);
    // Editor text fields and shortcuts remain owned by the old controller.
    PumpInput(false);
    if (!ui.EditingText()) {
        if (IsKeyPressed(KEY_TAB))
            context_->ProcessKeyDown(Rml::Input::KI_TAB, Modifiers());
        if (auto* focus = context_->GetFocusElement();
            focus && focus->GetTagName() == "button" &&
            (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
            focus->Click();
            input.editor.enter = false;
        }
    }
    // RmlUi dispatches a click on mouse release, while the legacy
    // controller responds on mouse press. Consume the entire native
    // chrome region on mouse DOWN using the actual RmlUi layout, so the
    // old BACK and rate hitboxes cannot run before the RmlUi event.
    bool chromePointer = editorPresenter_.ConsumedPointer();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = GetMousePosition();
        const auto over = [&](const char* id) {
            auto* element = editorDocument_->GetElementById(id);
            if (!element) return false;
            const float x = element->GetAbsoluteLeft();
            const float y = element->GetAbsoluteTop();
            return mouse.x >= x && mouse.x < x + element->GetOffsetWidth() &&
                   mouse.y >= y && mouse.y < y + element->GetOffsetHeight();
        };
        chromePointer |= over("editor-header") ||
                         over("editor-footer-playback") ||
                         over("editor-footer-rate-time") ||
                         (ui.rateDropdownOpen && over("editor-rate-popup"));
        if (chromePointer) input.backPressed = false;
    }
    if (chromePointer) {
        input.editor.mouseClick = false;
        input.editor.mouseHeld = false;
        input.editor.mouseReleased = false;
        input.editor.mouseWheel = 0.0f;
        input.startPressed = false;
    }
    editorPresenter_.Sync(ui, title, dirty, rate, playing, songTimeMs);
    context_->Update();
    editorPresenter_.EndFrame();
}
void NativeMenuUi::SyncEditor(const EditorUiState& ui,
                              const std::string& title, bool dirty,
                              double rate, bool playing,
                              long long songTimeMs) {
    if (!ShowingEditor() || !EditorAvailable()) return;
    editorPresenter_.Sync(ui, title, dirty, rate, playing, songTimeMs);
    context_->Update();
}

void NativeMenuUi::UpdateSettings(GameSettings& settings, SettingsUi& ui,
                                  FrameInput& input, MenuEvents& events,
                                  bool exclusiveKeyboardInput) {
    if (!SettingsAvailable()) return;
    Activate(Page::Settings);
    currentLibrary_ = nullptr;
    currentUi_ = nullptr;
    settingsPresenter_.BeginFrame(settings, ui, input, events);
    settingsPresenter_.Sync();
    const bool keyboard = !exclusiveKeyboardInput &&
        ui.capturing < 0 && !ui.editingGlobalOffset && !ui.calibrationOpen;
    PumpInput(keyboard);
    if (keyboard) {
        if (auto* focus = context_->GetFocusElement();
            focus && focus->GetTagName() == "button" &&
            (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
            focus->Click();
    }
    // A focused Settings action must not trigger global menu shortcuts.
    if (ui.capturing >= 0 || ui.editingGlobalOffset || ui.calibrationOpen) {
        input.startPressed = false;
        input.backPressed = false;
        input.requestedScreen.reset();
        input.fullscreenPressed = false;
    } else if (auto* focus = context_->GetFocusElement();
               focus && focus->GetTagName() == "button") {
        input.startPressed = false;
    }
    settingsPresenter_.Sync();
    context_->Update();
    settingsPresenter_.EndFrame();
}

void NativeMenuUi::UpdateHome(FrameInput& input, const std::string& status) {
    if (!HomeAvailable()) return;
    Activate(Page::Home);
    currentUi_ = nullptr;
    currentLibrary_ = nullptr;
    pendingHomeAction_ = -1;
    const bool failed = status.find("FAILED") != std::string::npos ||
        status.find("UNAVAILABLE") != std::string::npos ||
        status.find("NOT SAVED") != std::string::npos;
    const std::string notice = failed ? status : "";
    if (notice != lastHomeNotice_) {
        if (auto* element = homeDocument_->GetElementById("home-notice"))
            element->SetInnerRML(Escape(notice));
        lastHomeNotice_ = notice;
    }
    PumpInput();
    // RmlUi activates focused buttons with Space; support Enter as well,
    // without allowing the Home default shortcut to steal focus activation.
    if (auto* focus = context_->GetFocusElement();
        focus && focus->GetTagName() == "button" &&
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
        focus->Click();
    switch (pendingHomeAction_) {
        case 0: input.requestedScreen = Screen::SongSelect; break;
        case 1: input.requestedScreen = Screen::Editor; break;
        case 2: input.requestedScreen = Screen::Skin; break;
        case 3: input.requestedScreen = Screen::Settings; break;
        case 4: input.closeRequested = true; break;
        default: break;
    }
    // Enter on a focused button cannot double-trigger the Home PLAY shortcut.
    const auto* focus = context_->GetFocusElement();
    if (pendingHomeAction_ >= 0 ||
        (focus && focus->GetTagName() == "button"))
        input.startPressed = false;
    context_->Update();
}

MenuEvents NativeMenuUi::Update(const SongLibrary& library, SongSelectUi& ui,
                                FrameInput& input) {
    MenuEvents result;
    if (!Available()) return result;
    if (visiblePage_ != Page::Play) {
        Activate(Page::Play);
        if (auto* search = document_->GetElementById("search"))
            static_cast<Rml::ElementFormControlInput*>(search)->
                SetValue(ui.search);
    }
    currentLibrary_ = &library;
    currentUi_ = &ui;
    pending_ = {};
    Rebuild(library, ui);
    PumpInput();
    if (auto* focus = context_->GetFocusElement();
        focus && focus->GetTagName() == "button" &&
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
        focus->Click();
    const bool typing = IsSearchFocused();
    if (!typing) {
        if (IsKeyPressed(KEY_UP)) ui.MoveTree(-1, library);
        if (IsKeyPressed(KEY_DOWN)) ui.MoveTree(1, library);
        if (IsKeyPressed(KEY_LEFT)) ui.MoveChart(-1, library);
        if (IsKeyPressed(KEY_RIGHT)) ui.MoveChart(1, library);
        // Enter on a focused EDIT button cannot start the chart as well.
        const auto* focus = context_->GetFocusElement();
        const bool buttonFocused = focus && focus->GetTagName() == "button";
        if (!buttonFocused && IsKeyPressed(KEY_ENTER))
            pending_.playSelected = true;
        if (!buttonFocused && IsKeyPressed(KEY_E))
            pending_.editSelected = true;
        if (IsKeyPressed(KEY_ESCAPE)) input.backPressed = true;
    } else {
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (auto* element = document_->GetElementById("search"))
                element->Blur();
        }
    }
    if (IsKeyPressed(KEY_F5)) pending_.refresh = true;
    // Keyboard actions from PollInput must not bypass RmlUi focus.
    input.startPressed = false;
    if (typing) {
        input.backPressed = false;
        input.openEditorPressed = false;
        input.requestedScreen.reset();
    }
    if (pending_.backRequested) input.backPressed = true;
    Rebuild(library, ui);
    context_->Update();
    result = pending_;
    pending_ = {};
    return result;
}

void NativeMenuUi::Render() {
    if (!context_ || visiblePage_ == Page::None) return;
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    context_->Render();
    renderer_.EnableScissorRegion(false);
    EndBlendMode();
}

} // namespace locus::app
