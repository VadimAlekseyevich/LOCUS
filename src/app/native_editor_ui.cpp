#include "app/native_editor_ui.h"

#include "app/app_input.h"
#include "app/chart_editor_ui.h"
#include "app/chart_editor_layout.h"
#include "app/editor_comparison.h"
#include "app/game_settings.h"
#include "app/rml_ui_text.h"
#include "app/rml_modal_focus.h"
#include "app/rml_glyph_alignment.h"

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
bool ParseSongTime(const std::string& raw,long long& result) noexcept {
    if (raw.empty() || raw.size()>9) return false;
    const bool negative=raw.front()=='-';
    if (negative && raw.size()==1) return false;
    long long value=0;
    for (std::size_t i=negative?1:0;i<raw.size();++i) {
        const unsigned char c=raw[i];
        if (c<'0' || c>'9') return false;
        value=value*10+c-'0';
    }
    result=negative?-value:value;
    return true;
}
} // namespace

void NativeEditorUi::Attach(Rml::ElementDocument* document) noexcept {
    document_ = document;
    textCache_.clear();
    iconLabels_.clear();
    glyphAlignment_.clear();
    if (document_) document_->GetElementsByClassName(iconLabels_,"editor-icon-label");
    activeSetId_.clear();
    setList_={};
    compareList_={};
    compareVisible_ = false;
    // Attach also runs when returning to an existing hidden document.
    // Reset the real layer along with the cached visibility state.
    if (document_) SetRmlModalVisibility(document_->GetElementById("editor-compare-layer"),false);
    tempoRows_ = -1;
    lastTab_ = -1;
    lastTextEditing_ = -1;
}
void NativeEditorUi::BeginFrame(EditorUiState& ui, FrameInput& input) noexcept {
    ui_ = &ui;
    input_ = &input;
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
    SetText("editor-play-label", running ? u8"\u2016" : u8"\u25B6");
    for (auto* label:iconLabels_) AlignRmlGlyph(*label,glyphAlignment_[label]);
    SetText("editor-waveform-label",ui.waveformVisible?"WAVE ON":"WAVE OFF");
    if (auto* wave=document_->GetElementById("editor-waveform"))
        wave->SetClass("selected",ui.waveformVisible);
    if (auto* metronome=document_->GetElementById("editor-metronome"))
        metronome->SetClass("selected",ui.metronomeEnabled);
    lastRatePercent_ = std::clamp(
        static_cast<int>(std::lround(rate * 100.0)), 1, 100);
    if (auto* control = dynamic_cast<Rml::ElementFormControlInput*>(
            document_->GetElementById("editor-preview-rate"))) {
        const std::string number = std::to_string(lastRatePercent_);
        if (control != document_->GetContext()->GetFocusElement() &&
            control->GetValue() != number)
            control->SetValue(number);
    }
    lastSongTimeMs_=timeMs;
    if (auto* field=dynamic_cast<Rml::ElementFormControlInput*>(
            document_->GetElementById("editor-preview-time"))) {
        const auto value=std::to_string(timeMs);
        if (field!=document_->GetContext()->GetFocusElement() &&
            field->GetValue()!=value) field->SetValue(value);
        if (draft && !draft->HasSongDuration()) field->SetAttribute("disabled","");
        else field->RemoveAttribute("disabled");
    }
    if (settings) {
        SetText("editor-speed-label", std::to_string(
            static_cast<int>(std::lround(settings->editorPreviewSpeed))) + " px/s");
        SetText("editor-path-label", std::to_string(
            static_cast<int>(std::lround(settings->editorPreviewLengthMs))) + " ms");
    }
    if (draft && ui.tab == EditorTab::Route) {
        SetText("editor-snap-label", "SNAP 1/" + std::to_string(draft->Data().route.snapDivision));
        SetText("editor-zoom-label", "ZOOM " + std::to_string(static_cast<int>(std::lround(detail::PixelsPerBeat(ui)/36.0f*100))) + "%");
        const auto& counts=draft->DirectionPressCounts();
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
        if (auto* status = document_->GetElementById("editor-status-slot"))
            status->SetProperty("display", ui.tab == EditorTab::Set ? "block" : "none");
        lastTab_ = static_cast<int>(ui.tab);
    }
    const bool showCompare = ui.tab == EditorTab::Route && ui.comparisonPickerOpen && setView;
    if (auto* toggle = document_->GetElementById("editor-tool-compare")) {
        toggle->SetClass("selected",showCompare);
        if (setView && CanOpenEditorComparison(*setView))
            toggle->RemoveAttribute("disabled");
        else toggle->SetAttribute("disabled","");
    }
    if (showCompare != compareVisible_) {
        SetRmlModalVisibility(document_->GetElementById("editor-compare-layer"),showCompare);
        compareVisible_ = showCompare;
        if (showCompare) {
            if (auto* list = document_->GetElementById("editor-compare-list"))
                list->SetScrollTop(0);
        }
    }
    if (showCompare) {
        SyncDifficultyList(ui,*setView,title,true);
        SetText("editor-compare-notice",std::to_string(1+ui.comparisonFiles.size())+" / 3 OPEN");
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
        SyncDifficultyList(ui,*setView,title,false);
    }
}

void NativeEditorUi::SyncDifficultyList(const EditorUiState& ui,
    const EditorSetView& view,const std::string& title,bool compare) {
    auto* scroll=document_->GetElementById(compare?"editor-compare-list":"editor-set-list");
    auto* list=document_->GetElementById(compare?"editor-compare-rows":"editor-set-rows");
    auto* context=document_->GetContext();
    if (!scroll || !list || !context) return;
    auto& state=compare?compareList_:setList_;
    const float density=context->GetDensityIndependentPixelRatio();
    constexpr float gap=5.0f;
    const char* rowClass=compare?"editor-compare-row":"editor-set-row";
    if (state.identity!=view.id) {
        state={};
        state.identity=view.id;
        scroll->SetScrollTop(0);
    }
    if (state.stepDp<=0) state.stepDp=(compare?34.0f:30.0f)+gap;
    for (int i=0;i<list->GetNumChildren();++i) {
        auto* row=list->GetChild(i);
        if (row && row->IsClassSet(rowClass) && row->GetOffsetHeight()>0) {
            const float measured=LibraryScrollDp(row->GetOffsetHeight(),density)+gap;
            if (std::abs(measured-state.stepDp)>0.5f) {
                state.stepDp=measured;
                state.valid=false;
            }
            break;
        }
    }
    const std::size_t draftRows=!compare && view.newDifficultyDraft?1:0;
    const std::size_t count=compare?view.comparisonOptions.size():
        view.difficulties.size()+draftRows;
    const float height=LibraryScrollDp(scroll->GetClientHeight(),density);
    const float top=std::clamp(LibraryScrollDp(scroll->GetScrollTop(),density),
        0.0f,std::max(0.0f,count*state.stepDp-gap-height));
    const auto window=VisibleUniformRows(count,top,state.stepDp,
        VirtualRowsForViewport(height,state.stepDp),5,gap);
    // Only visible identity/order matters. Avoid hashing thousands of
    // offscreen paths/IDs on every Sync. Labels and selection update in place.
    std::ostringstream signature;
    signature<<draftRows<<':'<<count<<':';
    for (std::size_t row=window.first;row<window.end;++row) {
        if (row<draftRows) { signature<<"draft;"; continue; }
        const auto i=row-draftRows;
        const auto key=compare?view.comparisonOptions[i].file.u8string():
            (i<view.difficultyIds.size()?view.difficultyIds[i]:std::string{});
        signature<<key.size()<<':'<<key;
    }
    const auto identity=signature.str();
    if (!state.valid || state.window.first!=window.first ||
        state.window.end!=window.end || state.visibleSignature!=identity) {
        std::ostringstream html;
        const auto spacer=[&](float dp) {
            if(dp>0) html<<"<div class='editor-list-spacer' style='height:"<<dp
                <<"dp;flex:0 0 "<<dp<<"dp'></div>";
        };
        spacer(window.before);
        for (std::size_t row=window.first;row<window.end;++row) {
            if (row<draftRows) {
                html<<"<button class='editor-set-row' id='editor-draft-row' data-action='draft-difficulty'><span class='button-label'></span></button>";
                continue;
            }
            const auto i=row-draftRows;
            if(compare) {
                html<<"<button class='editor-compare-row' id='editor-compare-row-"<<i
                    <<"' data-action='compare-select' data-file='"
                    <<EscapeRml(view.comparisonOptions[i].file.u8string())
                    <<"'><span class='button-label'></span></button>";
            } else {
                html<<"<button class='editor-set-row' id='editor-difficulty-"<<i
                    <<"' data-action='set-difficulty' data-id='"
                    <<EscapeRml(i<view.difficultyIds.size()?view.difficultyIds[i]:std::string{})
                    <<"'><span class='button-label'></span></button>";
            }
        }
        spacer(window.after);
        // Invalidate only recycled row text; do not grow a cache of all rows.
        for(int i=0;i<list->GetNumChildren();++i)
            if(auto* child=list->GetChild(i)) textCache_.erase(child->GetId());
        for(auto* focus=context->GetFocusElement();focus;focus=focus->GetParentNode()) {
            if(focus==list) { document_->Focus(); break; }
        }
        list->SetInnerRML(html.str());
        scroll->SetScrollTop(LibraryScrollPixels(top,density));
        state.window=window;
        state.visibleSignature=identity;
        state.valid=true;
    }
    for(std::size_t row=window.first;row<window.end;++row) {
        if(row<draftRows) {
            if(auto* button=document_->GetElementById("editor-draft-row")) {
                button->SetClass("selected",view.selected<0);
                SetText("editor-draft-row",title+" (DRAFT)");
            }
            continue;
        }
        const auto i=row-draftRows;
        const auto id=std::string(compare?"editor-compare-row-":"editor-difficulty-")+std::to_string(i);
        auto* button=document_->GetElementById(id);
        if(!button) continue;
        const bool active=static_cast<int>(i)==view.current;
        if(compare) {
            const auto& option=view.comparisonOptions[i];
            const bool included=HasEditorComparison(ui,option.file);
            button->SetClass("selected",included||active);
            if(active || !option.available || option.file.empty() ||
               (!included && ui.comparisonFiles.size()>=2))
                button->SetAttribute("disabled","");
            else button->RemoveAttribute("disabled");
            const auto& name=i<view.difficulties.size()?view.difficulties[i]:option.title;
            SetText(id.c_str(),std::string(active?"EDIT  ":included?"−  ":"+  ")+name);
        } else {
            button->SetClass("active",active);
            button->SetClass("selected",static_cast<int>(i)==view.selected);
            SetText(id.c_str(),view.difficulties[i]);
        }
    }
}
void NativeEditorUi::CancelFocusedField() {
    if (!document_) return;
    if (auto* field = dynamic_cast<Rml::ElementFormControlInput*>(document_->GetContext()->GetFocusElement())) {
        field->SetValue(field->GetId() == "editor-preview-rate"
            ? std::to_string(lastRatePercent_) : field->GetId()=="editor-preview-time"
                ? std::to_string(lastSongTimeMs_) : focusedInitialValue_);
        focusedInitialValue_=field->GetValue();
        field->Blur();
    }
}
void NativeEditorUi::ProcessEvent(Rml::Event& event) {
    if (!ui_ || !input_) return;
    if (auto* field=dynamic_cast<Rml::ElementFormControlInput*>(event.GetTargetElement());
        field && field->GetId()=="editor-preview-time") {
        const auto type=event.GetType();
        if (type=="focus") {
            focusedInitialValue_=field->GetValue();
            field->Select();
        } else if (type=="click") field->Select();
        else if (type=="blur") {
            long long ms=0;
            if (field->GetValue()!=focusedInitialValue_ &&
                ParseSongTime(field->GetValue(),ms) && ms!=lastSongTimeMs_)
                input_->editor.seekSongTimeMs=ms;
            field->SetValue(std::to_string(lastSongTimeMs_));
        }
        return;
    }
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
    // Modal whitespace/scrollbar clicks do nothing; the backdrop alone
    // dismisses. Never let an underlying focused control emit a command.
    if (ui_->comparisonPickerOpen && action.rfind("compare-",0)!=0) return;
    if (action == "compare-toggle") {
        input_->editor.compareAction = ComparePickerAction::Toggle;
    } else if (action == "compare-dismiss") {
        input_->editor.compareAction = ComparePickerAction::Dismiss;
    } else if (action == "compare-select") {
        input_->editor.compareAction = ComparePickerAction::Select;
        input_->editor.compareFile = std::filesystem::u8path(
            target->GetAttribute<Rml::String>("data-file",""));
    } else if (action == "draft-difficulty") {
        input_->editor.selectDraft = true;
    } else if (action == "attach-audio") {
        input_->editor.attachAudio = true;
    } else if (action == "tempo-add") {
        input_->editor.tempoAdd = true;
    } else if (action == "tempo-remove") {
        input_->editor.tempoRemove = true;
        input_->editor.tempoRow = target->GetAttribute<int>("data-index", -1);
    } else if (action == "waveform-toggle" && ui_->tab==EditorTab::Route) {
        input_->editor.waveformToggle=true;
    } else if (action == "metronome-toggle") {
        input_->editor.metronomeToggle=true;
    } else if (action == "preview-play") {
        ui_->rateDropdownOpen = false;
        input_->editor.previewToggle = true;
    } else if (action == "preview-stop") {
        ui_->rateDropdownOpen = false;
        input_->editor.previewStop = true;
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
}
} // namespace locus::app
