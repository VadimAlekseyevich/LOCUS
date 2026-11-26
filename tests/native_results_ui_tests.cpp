#include "app/native_results_ui.h"
#include "app/menu_controller.h"
#include "app/rml_native_files.h"
#include "rml_test_support.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <iostream>

namespace {
using namespace locus::app;
bool Check(bool ok,const char* label) {
    if (!ok) std::cerr << "NATIVE_RESULTS_FAIL: " << label << '\n';
    return ok;
}
class ResultEvents final : public Rml::EventListener {
public:
    explicit ResultEvents(NativeResultsUi& presenter) : presenter_(presenter) {}
    void ProcessEvent(Rml::Event& event) override { presenter_.ProcessEvent(event); }
private:
    NativeResultsUi& presenter_;
};
}

bool TestNativeResultsVirtualHistory() {
    using namespace locus::app;
    RmlNativeFiles files;
    locus::tests::NullRenderer renderer;
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    if (!Check(Rml::Initialise(),"RmlUi initializes")) return false;
    bool ok=Check(locus::tests::TestFontAvailable(),"font loads");
    auto* context=Rml::CreateContext("results-virtual-test",{1536,960});
    auto* document=context ? context->LoadDocument("assets/ui/rml/results.rml") : nullptr;
    ok &= Check(document!=nullptr,"production RESULTS document loads");
    if (document) {
        SongSelectUi history;
        RunResult selected;
        selected.id="attempt";
        selected.songSetId="test-set";
        selected.difficultyId="test-chart";
        selected.hasReplay=true;
        selected.grades={19,0,0,1};
        selected.accuracyPercent=95.0;
        selected.notesJudged=20;
        FrameInput input;
        MenuEvents events;
        NativeResultsUi presenter;
        ResultEvents listener(presenter);
        document->AddEventListener("click",&listener);
        context->SetDensityIndependentPixelRatio(1.6f);
        document->Show();
        context->Update();
        presenter.Attach(document);
        presenter.BeginFrame(&selected,selected.grades,0,"test",true,history,input,events);
        presenter.Sync();
        context->Update();
        presenter.Sync();
        context->Update();
        auto* watch=document->GetElementById("results-watch-replay");
        if (watch) watch->Click();
        ok &= Check(watch && events.watchReplay,
                    "historical attempt can emit watch replay without retry");
        events.watchReplay=false;
        auto* error=document->GetElementById("results-replay-status");
        ok &= Check(error && error->GetOffsetHeight()==0 &&
                    !document->GetElementById("results-notes") &&
                    watch && watch->GetOffsetHeight()>0,
                    "no redundant replay status or notes count; watch has a visible action");
        auto* rank=document->GetElementById("results-rank");
        ok &= Check(rank && rank->IsClassSet("rank-S") && rank->GetOffsetHeight()>0 &&
                    !document->GetElementById("results-history") &&
                    !document->GetElementById("results-edit"),
                    "rank replaces other results and Edit");
        presenter.EndFrame();
        selected.grades={20,0,0,0};
        selected.accuracyPercent=100.0;
        presenter.BeginFrame(&selected,selected.grades,20,"test",false,history,input,events);
        presenter.Sync();
        context->Update();
        ok &= Check(rank && rank->IsClassSet("rank-SS") && !rank->IsClassSet("rank-S"),
                    "rank updates color and glow class when selection changes");
        presenter.EndFrame();
        events.watchReplay=false;
        if (watch) watch->Click();
        ok &= Check(!events.watchReplay,"no event uses expired frame state");
        presenter.Attach(nullptr);
        document->RemoveEventListener("click",&listener);
        document->Hide();
    }
    if (context) Rml::RemoveContext("results-virtual-test");
    Rml::Shutdown();
    return ok;
}
