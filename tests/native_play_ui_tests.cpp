#include "app/native_play_ui.h"
#include "app/rml_native_files.h"
#include "rml_test_support.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <iostream>

namespace {
using namespace locus::app;

bool Check(bool ok, const char* label) {
    if (!ok) std::cerr << "NATIVE_PLAY_FAIL: " << label << '\n';
    return ok;
}

class PlayEvents final : public Rml::EventListener {
public:
    explicit PlayEvents(NativePlayUi& presenter) : presenter_(presenter) {}
    void ProcessEvent(Rml::Event& event) override { presenter_.ProcessEvent(event); }
private:
    NativePlayUi& presenter_;
};

Rml::Element* ChartRow(Rml::Element* tree, int song, int chart) {
    if (!tree) return nullptr;
    for (int i = 0; i < tree->GetNumChildren(); ++i) {
        auto* row = tree->GetChild(i);
        if (row && row->GetAttribute<int>("data-song",-1)==song &&
                   row->GetAttribute<int>("data-chart",-1)==chart)
            return row;
    }
    return nullptr;
}
} // namespace

// Exercise the actual presenter + RML event tree against the installed
// library fixture, not a separate mock browser or a GPU/window.
bool TestNativePlayPresenter(const locus::app::SongLibrary& library) {
    using namespace locus::app;
    RmlNativeFiles files;
    locus::tests::NullRenderer renderer;
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    if (!Check(Rml::Initialise(),"RmlUi initialises")) return false;
    bool ok = Check(locus::tests::TestFontAvailable(),"installed font loads");
    auto* context = Rml::CreateContext("play-presenter-test",{1536,960});
    auto* document = context ? context->LoadDocument("assets/ui/rml/play.rml") : nullptr;
    ok &= Check(document != nullptr,"production PLAY document loads");
    if (document) {
        context->SetDensityIndependentPixelRatio(1.6f);
        NativePlayUi presenter;
        PlayEvents listener(presenter);
        document->AddEventListener("click",&listener);
        document->AddEventListener("change",&listener);
        presenter.Attach(document);
        document->Show();
        context->Update();
        SongSelectUi selection;
        selection.SelectSong(1,library);
        selection.SelectChart(1,library);
        presenter.BeginFrame(library,selection,1.6f);
        presenter.Sync();
        context->Update();
        auto* tree = document->GetElementById("tree");
        auto* easy = ChartRow(tree,1,0);
        auto* hard = ChartRow(tree,1,1);
        ok &= Check(easy && hard && hard->IsClassSet("active"),
                    "installed selected chart has an active real DOM row");
        if (easy) easy->Click();
        const auto* originalEasy = easy;
        const auto* originalHard = hard;
        presenter.Sync();
        context->Update();
        easy = ChartRow(tree,1,0);
        hard = ChartRow(tree,1,1);
        ok &= Check(selection.selectedSong==1 && selection.selectedChart==0 &&
                    easy && easy->IsClassSet("active") && hard &&
                    !hard->IsClassSet("active") && easy==originalEasy &&
                    hard==originalHard,
                    "difficulty click changes active class without replacing DOM nodes");
        auto* start = document->GetElementById("start");
        if (start) start->Click();
        const auto play = presenter.EndFrame();
        ok &= Check(start && play.playSelected && !play.editSelected,
                    "native PLAY emits only the typed Play command");

        // An event after EndFrame must not use borrowed frame model pointers.
        hard = ChartRow(tree,1,1);
        if (hard) hard->Click();
        ok &= Check(selection.selectedChart==0,"events outside frame cannot mutate model");
        presenter.BeginFrame(library,selection,1.6f);
        auto* edit = document->GetElementById("edit");
        if (edit) edit->Click();
        const auto editing = presenter.EndFrame();
        ok &= Check(edit && editing.editSelected && !editing.playSelected,
                    "native EDIT emits only the typed Edit command");

        // Keyboard uses the same collected shortcut flags as the fallback,
        // regardless of the physical key configured in GameSettings.
        presenter.BeginFrame(library,selection,1.6f);
        FrameInput keyboard;
        keyboard.openEditorPressed = true;
        presenter.ProcessKeyboard(keyboard);
        const auto shortcutEdit = presenter.EndFrame();
        ok &= Check(shortcutEdit.editSelected && !shortcutEdit.playSelected &&
                    !keyboard.openEditorPressed,
                    "configured Edit shortcut becomes one typed action");
        presenter.BeginFrame(library,selection,1.6f);
        keyboard = {};
        keyboard.refreshLibraryPressed = true;
        presenter.ProcessKeyboard(keyboard);
        const auto shortcutRefresh = presenter.EndFrame();
        ok &= Check(shortcutRefresh.refresh && !keyboard.refreshLibraryPressed,
                    "configured Refresh shortcut is consumed");
        presenter.BeginFrame(library,selection,1.6f);
        ok &= Check(edit && edit->Focus(),"EDIT button accepts keyboard focus");
        keyboard = {};
        keyboard.startPressed = true;
        presenter.ProcessKeyboard(keyboard);
        const auto enterEdit = presenter.EndFrame();
        ok &= Check(enterEdit.editSelected && !enterEdit.playSelected &&
                    !keyboard.startPressed,
                    "focused EDIT consumes Enter without launching PLAY");

        presenter.BeginFrame(library,selection,1.6f);
        easy = ChartRow(tree,1,0);
        ok &= Check(easy && easy->Focus(),"selected difficulty accepts focus");
        keyboard = {};
        keyboard.startPressed = true;
        presenter.ProcessKeyboard(keyboard);
        const auto enterChart = presenter.EndFrame();
        ok &= Check(enterChart.playSelected && !enterChart.editSelected &&
                    !keyboard.startPressed && selection.selectedChart==0,
                    "Enter on focused chart starts selected difficulty");

        presenter.BeginFrame(library,selection,1.6f);
        Rml::Element* songRow = nullptr;
        if (tree) for (int i=0;i<tree->GetNumChildren();++i) {
            auto* row = tree->GetChild(i);
            if (row && row->GetAttribute<Rml::String>("data-action","")=="song" &&
                row->GetAttribute<int>("data-song",-1)==1) { songRow=row; break; }
        }
        ok &= Check(songRow && songRow->Focus(),"song row accepts focus");
        keyboard = {};
        keyboard.startPressed = true;
        presenter.ProcessKeyboard(keyboard);
        const auto enterSong = presenter.EndFrame();
        ok &= Check(enterSong.playSelected && !enterSong.editSelected,
                    "Enter on focused song starts selected difficulty");

        presenter.BeginFrame(library,selection,1.6f);
        auto* search = dynamic_cast<Rml::ElementFormControlInput*>(document->GetElementById("search"));
        ok &= Check(search && search->Focus(),"native search can be focused");
        if (search) {
            for (char c : std::string("zzzzzzzz"))
                context->ProcessTextInput(static_cast<Rml::Character>(c));
        }
        presenter.Sync();
        context->Update();
        ok &= Check(search && search->GetValue()=="zzzzzzzz",
                    "RmlUi text widget receives the typed characters");
        ok &= Check(selection.search=="zzzzzzzz" && tree &&
                    !ChartRow(tree,1,0) && !ChartRow(tree,1,1),
                    "native text input filters the real library");
        if (search) {
            context->ProcessKeyDown(Rml::Input::KI_A,Rml::Input::KM_CTRL);
            context->ProcessKeyUp(Rml::Input::KI_A,Rml::Input::KM_CTRL);
            context->ProcessKeyDown(Rml::Input::KI_BACK,0);
            context->ProcessKeyUp(Rml::Input::KI_BACK,0);
        }
        presenter.Sync();
        context->Update();
        ok &= Check(selection.search.empty() && ChartRow(tree,1,0) &&
                    ChartRow(tree,1,1),
                    "clearing the real search widget restores both difficulties");
        keyboard = {};
        keyboard.startPressed = true;
        keyboard.openEditorPressed = true;
        keyboard.refreshLibraryPressed = true;
        keyboard.backPressed = true;
        presenter.ProcessKeyboard(keyboard);
        const auto searchOnly = presenter.EndFrame();
        ok &= Check(!searchOnly.playSelected && !searchOnly.editSelected &&
                    !searchOnly.refresh && !keyboard.backPressed &&
                    !keyboard.startPressed && !keyboard.openEditorPressed &&
                    !keyboard.refreshLibraryPressed,
                    "search owns activation, rebindable shortcuts and navigation");
        presenter.Attach(nullptr);
        document->RemoveEventListener("click",&listener);
        document->RemoveEventListener("change",&listener);
        document->Hide();
    }
    if (context) Rml::RemoveContext("play-presenter-test");
    Rml::Shutdown();
    return ok;
}
