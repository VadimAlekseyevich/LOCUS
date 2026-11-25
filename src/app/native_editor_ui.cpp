#include "app/native_editor_ui.h"

#include "app/app_input.h"
#include "app/chart_editor_ui.h"
#include "app/chart_editor_layout.h"
#include "app/game_settings.h"
#include "app/rml_ui_text.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Context.h>
#include <sstream>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include "core/tempo_map.h"

namespace locus::app {
namespace {
std::string EscapeRml(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
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
bool ParsePercent(const std::string& raw, int& result) noexcept {
    if (raw.empty() || raw.size() > 3) return false;
    int value = 0;
    for (const unsigned char c : raw) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + static_cast<int>(c - '0');
    }
    result = std::clamp(value, 1, 100);
    return true;
}
} // namespace

void NativeEditorUi::Attach(Rml::ElementDocument* document) noexcept {
    document_ = document;
    textCache_.clear();
    suppressClick_ = false;
    activeSetId_.clear();
    setListSignature_.clear();
    tempoRows_ = -1;
    lastTab_ = -1;
    lastTextEditing_ = -1;
}
void NativeEditorUi::BeginFrame(EditorUiState& ui, FrameInput& input) noexcept {
    ui_ = &ui;
    input_ = &input;
    consumedPointer_ = false;
}
void NativeEditorUi::EndFrame() noexcept {
    ui_ = nullptr;
    input_ = nullptr;
}
void NativeEditorUi::SetText(const char* id, const std::string& value) {
    if (!document_) return;
    const auto old = textCache_.find(id);
    if (old != textCache_.end() && old->second == value) return;
    if (auto* element = document_->GetElementById(id)) {
        UpdateRmlText(element, value);
        textCache_[id] = value;
    }
}
void NativeEditorUi::Sync(const EditorUiState& ui, const std::string& title,
                          bool dirty, double rate, bool running,
                          long long timeMs, const core::ChartEditor* draft,
                          const GameSettings* settings,
                          const EditorSetView* setView) {
    if (!document_) return;
    // Song Setup shows the selected name; authoring panes show chart names.
    // No global screen header is needed.
    (void)dirty;
    SetText("editor-preview-play", running ? "PAUSE" : "PLAY");
    lastRatePercent_ = std::clamp(
        static_cast<int>(std::lround(rate * 100.0)), 1, 100);
    if (auto* control = dynamic_cast<Rml::ElementFormControlInput*>(
            document_->GetElementById("editor-preview-rate"))) {
        const std::string number = std::to_string(lastRatePercent_);
        if (control != document_->GetContext()->GetFocusElement() &&
            control->GetValue() != number)
            control->SetValue(number);
    }
    SetText("editor-preview-time", std::to_string(timeMs) + " MS");
    if (settings) {
        SetText("editor-speed-label", std::to_string(
            static_cast<int>(std::lround(settings->editorPreviewSpeed))) + " px/s");
        SetText("editor-path-label", std::to_string(
            static_cast<int>(std::lround(settings->editorPreviewLengthMs))) + " ms");
    }
    if (draft && ui.tab == EditorTab::Route) {
        SetText("editor-snap-label", "SNAP 1/" + std::to_string(draft->Data().route.snapDivision));
        SetText("editor-zoom-label", "ZOOM " + std::to_string(static_cast<int>(std::lround(detail::PixelsPerBeat(ui)/36.0f*100))) + "%");
        const auto counts = detail::DirectionPressCounts(draft->Data().route);
        const auto* selected = draft->SelectedTurn();
        for (int index=0; index<8; ++index) {
            const std::string suffix = std::to_string(index);
            SetText(("editor-count-" + suffix).c_str(),
                std::to_string(counts[static_cast<std::size_t>(index)]));
            if (auto* button = document_->GetElementById("editor-pad-" + suffix))
                button->SetClass("selected", selected &&
                    static_cast<int>(selected->direction) == index);
        }
    }
    if (lastTab_ != static_cast<int>(ui.tab)) {
        if (auto* route = document_->GetElementById("editor-route-ui"))
            route->SetProperty("display", ui.tab == EditorTab::Route ? "flex" : "none");
        if (auto* set = document_->GetElementById("editor-set-ui"))
            set->SetProperty("display", ui.tab == EditorTab::Set ? "flex" : "none");
        lastTab_ = static_cast<int>(ui.tab);
    }
    if (setView && ui.tab == EditorTab::Set) {
        activeSetId_ = setView->id;
        auto* focus = document_->GetContext()->GetFocusElement();
        const auto update = [&](const char* id, const std::string& value) {
            auto* input = dynamic_cast<Rml::ElementFormControlInput*>(
                document_->GetElementById(id));
            if (!input) return;
            if (input != focus && input->GetValue() != value)
                input->SetValue(value);
            if (setView->managed) input->RemoveAttribute("disabled");
            else input->SetAttribute("disabled", "");
        };
        update("editor-set-title", setView->title);
        update("editor-set-artist", setView->artist);
        update("editor-set-mapper", setView->mapper);
        std::string tags;
        for (const auto& tag : setView->tags) {
            if (!tags.empty()) tags += ", ";
            tags += tag;
        }
        update("editor-set-tags", tags);
        const auto selected = setView->selected;
        const bool selectedInstalled = selected >= 0 &&
            selected < static_cast<int>(setView->difficulties.size());
        const auto setInput = [&](const std::string& id, const std::string& value, bool enabled) {
            if (auto* field = dynamic_cast<Rml::ElementFormControlInput*>(document_->GetElementById(id))) {
                if (field != focus && field->GetValue() != value) field->SetValue(value);
                if (enabled) field->RemoveAttribute("disabled");
                else field->SetAttribute("disabled", "");
            }
        };
        setInput("editor-difficulty-title", selectedInstalled
            ? setView->difficulties[static_cast<std::size_t>(selected)] : title,
            selectedInstalled || setView->newDifficultyDraft);
        SetText("editor-set-hint", setView->managed ? "" :
                "ATTACH AUDIO, THEN OPEN THE DRAFT TO CREATE THE SET");
        SetText("editor-attach-audio", setView->hasAudio ? "AUDIO LOADED / CHANGE" : "ATTACH AUDIO");
        SetText("editor-audio-name", setView->audioName.empty() && draft
            ? draft->Data().audioPath : setView->audioName);
        setInput("editor-preview-ms", setView->managed ? std::to_string(setView->previewStartMs) : "",
            setView->managed);
        const auto tempo = !setView->tempoMap.empty() ? setView->tempoMap
            : draft ? core::EffectiveTempoMap(draft->Data().route, draft->Data().offsetSeconds)
                    : std::vector<core::TempoMarker>{};
        if (tempoRows_ != static_cast<int>(tempo.size())) {
            std::ostringstream html;
            for (std::size_t i = 0; i < tempo.size(); ++i) {
                html << "<div class='editor-tempo-row'>"
                    << "<input type='text' class='editor-set-input editor-tempo-offset' id='editor-tempo-ms-" << i
                    << "' data-kind='tempo-offset' data-index='" << i << "' maxlength='32' value='' />"
                    << "<input type='text' class='editor-set-input editor-tempo-bpm' id='editor-tempo-bpm-" << i
                    << "' data-kind='tempo-bpm' data-index='" << i << "' maxlength='32' value='' />";
                if (i == 0) html << "<div class='editor-tempo-action editor-tempo-start'>START</div>";
                else html << "<button class='editor-tempo-action' data-action='tempo-remove' data-index='" << i
                          << "'><span class='button-label'>DEL</span></button>";
                html << "</div>";
            }
            if (auto* list = document_->GetElementById("editor-tempo-list")) {
                list->SetInnerRML(html.str());
                tempoRows_ = static_cast<int>(tempo.size());
            }
        }
        for (std::size_t i = 0; i < tempo.size(); ++i) {
            const auto suffix = std::to_string(i);
            std::ostringstream bpm;
            bpm << std::setprecision(15) << tempo[i].bpm;
            setInput("editor-tempo-ms-" + suffix, std::to_string(tempo[i].timeMs), true);
            setInput("editor-tempo-bpm-" + suffix, bpm.str(), true);
        }
        // Only rebuild when row identity/order changes, not on selection or
        // title edits. This preserves keyboard focus and scrolling.
        std::ostringstream signature;
        signature << setView->id << ':' << setView->newDifficultyDraft;
        for (const auto& id : setView->difficultyIds) signature << ':' << id;
        if (signature.str() != setListSignature_) {
            setListSignature_ = signature.str();
            std::ostringstream html;
            if (setView->newDifficultyDraft)
                html << "<button class='editor-set-row' id='editor-draft-row' data-action='draft-difficulty'><span class='button-label'>"
                     << EscapeRml(title) << "</span></button>";
            for (std::size_t i=0; i<setView->difficulties.size(); ++i) {
                html << "<button class='editor-set-row' id='editor-difficulty-" << i
                     << "' data-action='set-difficulty' data-id='" << EscapeRml(setView->difficultyIds[i])
                     << "'><span class='button-label'>" << EscapeRml(setView->difficulties[i])
                     << "</span></button>";
                textCache_.erase("editor-difficulty-" + std::to_string(i));
            }
            textCache_.erase("editor-draft-row");
            if (auto* list = document_->GetElementById("editor-set-list"))
                list->SetInnerRML(html.str());
        }
        if (auto* row = document_->GetElementById("editor-draft-row")) {
            row->SetClass("selected", !selectedInstalled);
            SetText("editor-draft-row", title + " (DRAFT)");
        }
        for (std::size_t i=0; i<setView->difficulties.size(); ++i) {
            const auto id = "editor-difficulty-" + std::to_string(i);
            if (auto* row = document_->GetElementById(id)) {
                row->SetClass("active", static_cast<int>(i) == setView->current);
                row->SetClass("selected", static_cast<int>(i) == selected);
                SetText(id.c_str(), setView->difficulties[i]);
            }
        }
    }
}
void NativeEditorUi::CancelFocusedField() {
    if (!document_) return;
    if (auto* field = dynamic_cast<Rml::ElementFormControlInput*>(document_->GetContext()->GetFocusElement())) {
        field->SetValue(field->GetId() == "editor-preview-rate"
            ? std::to_string(lastRatePercent_) : focusedInitialValue_);
        field->Blur();
    }
}
void NativeEditorUi::ProcessEvent(Rml::Event& event) {
    if (!ui_ || !input_ || suppressClick_) return;
    // RmlUi emits "change" on every keystroke, not just on form commit.
    // Never clamp/rewrite the live value: deleting "100" to type "50" must
    // allow an empty input. Commit only on blur (Enter also blurs).
    if (auto* control = dynamic_cast<Rml::ElementFormControlInput*>(
            event.GetTargetElement());
        control && control->GetId() == "editor-preview-rate") {
        const auto type = event.GetType();
        if (type == "focus" || type == "click") {
            // Select after mouse-up as well as on keyboard focus: a
            // mousedown may move the caret and clear an earlier selection.
            control->Select();
            if (type == "click") consumedPointer_ = true;
        } else if (type == "blur") {
            int percent = 0;
            if (ParsePercent(control->GetValue(), percent)) {
                if (percent != lastRatePercent_)
                    input_->editor.speedPercent = percent;
                control->SetValue(std::to_string(percent));
            } else {
                // Empty or malformed input cancels, preserving the current
                // transport rate instead of forcing a digit mid-edit.
                control->SetValue(std::to_string(lastRatePercent_));
            }
        }
        return;
    }
    if (ui_->tab == EditorTab::Set) {
        if (auto* field = dynamic_cast<Rml::ElementFormControlInput*>(event.GetTargetElement())) {
            const auto kind = field->GetAttribute<Rml::String>("data-kind", "");
            if (kind == "set-field" || kind == "difficulty-title" ||
                kind == "preview-ms" || kind == "tempo-offset" || kind == "tempo-bpm") {
                if (event.GetType() == "focus") focusedInitialValue_ = field->GetValue();
                if (event.GetType() == "blur" && field->GetValue() != focusedInitialValue_) {
                    if (kind == "difficulty-title")
                        input_->editor.difficultyTitle = field->GetValue();
                    else if (kind == "preview-ms")
                        input_->editor.previewMsValue = field->GetValue();
                    else if (kind == "tempo-offset" || kind == "tempo-bpm") {
                        input_->editor.tempoRow = field->GetAttribute<int>("data-index", -1);
                        input_->editor.tempoBpm = kind == "tempo-bpm";
                        input_->editor.tempoValue = field->GetValue();
                    } else {
                        const int value = field->GetAttribute<int>("data-field", -1);
                        if (value >= static_cast<int>(EditorSetField::Title) &&
                            value <= static_cast<int>(EditorSetField::Tags)) {
                            input_->editor.metadataField = static_cast<EditorSetField>(value);
                            input_->editor.metadataValue = field->GetValue();
                            input_->editor.metadataSetId = activeSetId_;
                        }
                    }
                }
                if (event.GetType() == "click") consumedPointer_ = true;
                return;
            }
        }
    }
    if (event.GetType() != "click") return;
    auto* target = event.GetTargetElement();
    while (target && !target->HasAttribute("data-action"))
        target = target->GetParentNode();
    if (!target || target->HasAttribute("disabled")) return;
    const std::string action = target->GetAttribute<Rml::String>("data-action", "");
    if (action.empty()) return;
    consumedPointer_ = true;
    if (action == "draft-difficulty") {
        input_->editor.selectDraft = true;
    } else if (action == "attach-audio") {
        input_->editor.attachAudio = true;
    } else if (action == "tempo-add") {
        input_->editor.tempoAdd = true;
    } else if (action == "tempo-remove") {
        input_->editor.tempoRemove = true;
        input_->editor.tempoRow = target->GetAttribute<int>("data-index", -1);
    } else if (action == "preview-play") {
        ui_->rateDropdownOpen = false;
        input_->editor.previewToggle = true;
    } else if (action == "preview-restart") {
        ui_->rateDropdownOpen = false;
        input_->editor.previewRestart = true;
    } else if (action == "set-difficulty") {
        input_->editor.selectedDifficultyId =
            target->GetAttribute<Rml::String>("data-id", "");
    } else if (action == "set-new") input_->editor.setRequest = EditorSetAction::NewDifficulty;
    else if (action == "set-duplicate") input_->editor.setRequest = EditorSetAction::Duplicate;
    else if (action == "set-import") input_->editor.setRequest = EditorSetAction::Import;
    else if (action == "set-export") input_->editor.setRequest = EditorSetAction::Export;
    else if (action == "set-delete-chart") input_->editor.setRequest = EditorSetAction::DeleteDifficulty;
    else if (action == "set-delete-set") input_->editor.setRequest = EditorSetAction::DeleteSongSet;
    else if (action == "set-restore") input_->editor.setRequest = EditorSetAction::Restore;
    else if (action == "set-bg") input_->editor.setRequest = EditorSetAction::AttachBackground;
    else if (action == "set-remove-bg") input_->editor.setRequest = EditorSetAction::RemoveBackground;
    else if (action == "set-folder") input_->editor.setRequest = EditorSetAction::OpenFolder;
    else if (action == "snap-less") input_->editor.snapLess = true;
    else if (action == "snap-more") input_->editor.snapMore = true;
    else if (action == "zoom-in") input_->editor.zoomIn = true;
    else if (action == "zoom-out") input_->editor.zoomOut = true;
    else if (action == "mirror-h") input_->editor.mirrorHorizontal = true;
    else if (action == "mirror-v") input_->editor.mirrorVertical = true;
    else if (action == "rotate-ccw") input_->editor.rotateCounterclockwise = true;
    else if (action == "rotate-cw") input_->editor.rotateClockwise = true;
    else if (action == "test-chart") input_->editorTest = true;
    else if (action == "remove-note") input_->editor.removeTurn = true;
    else if (action == "note") {
        const int heading = target->GetAttribute<int>("data-heading", -1);
        if (heading >= 0 && heading < 8)
            input_->editor.placeHeading = static_cast<core::Heading>(heading);
    } else if (action == "speed-minus" || action == "speed-plus")
        input_->editor.previewSpeedDelta = action == "speed-plus" ? 1 : -1;
    else if (action == "path-minus" || action == "path-plus")
        input_->editor.previewLengthDelta = action == "path-plus" ? 1 : -1;
    else if (action == "time-copy") {
        ui_->rateDropdownOpen = false;
        input_->editor.copySongTime = true;
    }
}
} // namespace locus::app
