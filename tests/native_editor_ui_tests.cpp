#include "app/native_editor_ui.h"
#include "app/app_input.h"
#include "app/chart_editor_ui.h"
#include "app/rml_native_files.h"
#include "rml_test_support.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/EventListener.h>
#include <iostream>

namespace {
using namespace locus::app;
bool Check(bool ok,const char* name) {
    if(!ok) std::cerr<<"NATIVE_EDITOR_FAIL "<<name<<'\n';
    return ok;
}
class Events final : public Rml::EventListener {
public:
    explicit Events(NativeEditorUi& ui):ui_(ui){}
    void ProcessEvent(Rml::Event& event) override { ui_.ProcessEvent(event); }
private:
    NativeEditorUi& ui_;
};
}

bool TestNativeEditorVirtualLists() {
    using namespace locus::app;
    RmlNativeFiles files;
    locus::tests::NullRenderer renderer;
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    if(!Check(Rml::Initialise(),"initialize RmlUi")) return false;
    bool ok=Check(locus::tests::TestFontAvailable(),"load font");
    auto* context=Rml::CreateContext("editor-virtual-test",{1536,960});
    auto* document=context?context->LoadDocument("assets/ui/rml/editor.rml"):nullptr;
    ok &= Check(document!=nullptr,"load production editor");
    if(document) {
        constexpr int count=8193;
        EditorSetView view;
        view.managed=true;
        view.hasAudio=true;
        view.newDifficultyDraft=true;
        view.id="fixture-set";
        view.difficulties.reserve(count);
        view.difficultyIds.reserve(count);
        view.comparisonOptions.reserve(count);
        for(int i=0;i<count;++i) {
            const auto id=std::to_string(i);
            view.difficulties.push_back("Difficulty "+id);
            view.difficultyIds.push_back(id);
            view.comparisonOptions.push_back(
                {std::filesystem::u8path("fixture/chart-"+id+".json"),
                 view.difficulties.back(),true});
        }
        EditorUiState ui;
        ui.tab=EditorTab::Set;
        FrameInput input;
        NativeEditorUi presenter;
        Events events(presenter);
        document->AddEventListener("click",&events);
        document->AddEventListener("focus",&events,true);
        document->AddEventListener("blur",&events,true);
        context->SetDensityIndependentPixelRatio(1.6f);
        document->Show();
        context->Update();
        presenter.Attach(document);
        presenter.BeginFrame(ui,input);
        const auto sync=[&] {
            presenter.Sync(ui,"Draft",false,1,false,0,nullptr,nullptr,&view);
            context->Update();
            presenter.Sync(ui,"Draft",false,1,false,0,nullptr,nullptr,&view);
            context->Update();
        };
        sync();
        auto* scroll=document->GetElementById("editor-set-list");
        auto* list=document->GetElementById("editor-set-rows");
        ok &= Check(scroll && list && list->GetNumChildren()<60 &&
            document->GetElementById("editor-draft-row") &&
            document->GetElementById("editor-difficulty-0"),
            "SET first viewport retains draft and bounded nodes");
        const auto lastId="editor-difficulty-"+std::to_string(count-1);
        if(scroll && list) {
            scroll->SetScrollTop(scroll->GetScrollHeight());
            sync();
            auto* last=document->GetElementById(lastId);
            ok &= Check(last && list->GetNumChildren()<60 &&
                !document->GetElementById("editor-draft-row"),
                "SET last difficulty reachable without all DOM rows");
            if(last) {
                last->Click();
                ok &= Check(input.editor.selectedDifficultyId==view.difficultyIds.back(),
                    "SET recycled row emits stable difficulty ID");
                view.selected=count-1;
                view.difficulties.back()="Renamed <&>";
                sync();
                auto* updated=document->GetElementById(lastId);
                ok &= Check(updated==last && updated->IsClassSet("selected"),
                    "visible rename and selection preserve row identity");
            }
        }
        auto* time=dynamic_cast<Rml::ElementFormControlInput*>(
            document->GetElementById("editor-preview-time"));
        if (time) {
            ok &= Check(time->Focus(),"song time accepts focus");
            int first=-1,last=-1;
            Rml::String selection;
            time->GetSelection(&first,&last,&selection);
            ok &= Check(selection=="0","song time selects entire initial value");
            time->SetValue("");
            sync();
            ok &= Check(time->GetValue().empty(),"live sync preserves empty time edit");
            time->SetValue("7125");
            time->Blur();
            ok &= Check(input.editor.seekSongTimeMs==7125,"blur commits exact milliseconds");
            input.editor.seekSongTimeMs.reset();
            time->Focus();
            time->SetValue("12345");
            presenter.CancelFocusedField();
            ok &= Check(!input.editor.seekSongTimeMs && time->GetValue()=="0",
                        "cancel restores current time without seeking");
        } else ok &= Check(false,"song time is editable input");
        ui.tab=EditorTab::Route;
        sync();
        auto* wave=document->GetElementById("editor-waveform");
        auto* waveLabel=document->GetElementById("editor-waveform-label");
        ok &= Check(wave && waveLabel && wave->IsClassSet("selected") &&
                    wave->GetOffsetWidth()>0 && waveLabel->GetInnerRML()=="WAVE ON",
                    "waveform toggle visible and enabled by default");
        if (wave) wave->Click();
        ok &= Check(input.editor.waveformToggle,"waveform button emits semantic toggle");
        input.editor.waveformToggle=false;
        ui.waveformVisible=false;
        sync();
        ok &= Check(wave && waveLabel && !wave->IsClassSet("selected") &&
                    waveLabel->GetInnerRML()=="WAVE OFF","waveform toggle reflects hidden state");
        ui.comparisonPickerOpen=true;
        view.current=0;
        sync();
        scroll=document->GetElementById("editor-compare-list");
        list=document->GetElementById("editor-compare-rows");
        ok &= Check(scroll && list && list->GetNumChildren()<60 &&
            document->GetElementById("editor-compare-row-0") &&
            document->GetElementById("editor-compare-row-0")->HasAttribute("disabled"),
            "COMPARE active chart disabled and viewport bounded");
        if(scroll && list) {
            scroll->SetScrollTop(scroll->GetScrollHeight());
            sync();
            const auto id="editor-compare-row-"+std::to_string(count-1);
            auto* last=document->GetElementById(id);
            ok &= Check(last && list->GetNumChildren()<60,
                "COMPARE last difficulty reachable");
            if(last) {
                last->Click();
                ok &= Check(input.editor.compareAction==ComparePickerAction::Select &&
                    input.editor.compareFile==view.comparisonOptions.back().file,
                    "COMPARE recycled row emits stable path");
                ui.comparisonFiles={view.comparisonOptions[1].file,
                                    view.comparisonOptions[2].file};
                sync();
                last=document->GetElementById(id);
                ok &= Check(last && last->HasAttribute("disabled"),
                    "COMPARE retains three-pane limit after recycling");
                ui.comparisonFiles.clear();
            }
            // A different set must not inherit the previous huge scroll.
            view.id="other-set";
            view.comparisonOptions.resize(3);
            view.difficulties.resize(3);
            view.difficultyIds.resize(3);
            sync();
            ok &= Check(document->GetElementById("editor-compare-row-0") &&
                scroll->GetScrollTop()<1.0f && list->GetNumChildren()<10,
                "new set resets viewport and removes old rows");
        }
        presenter.EndFrame();
        presenter.Attach(nullptr);
        document->RemoveEventListener("click",&events);
        document->RemoveEventListener("focus",&events,true);
        document->RemoveEventListener("blur",&events,true);
        document->Hide();
    }
    if(context) Rml::RemoveContext("editor-virtual-test");
    Rml::Shutdown();
    return ok;
}
