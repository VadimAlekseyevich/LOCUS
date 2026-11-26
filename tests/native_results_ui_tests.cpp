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
Rml::Element* FindRow(Rml::Element* list,int index) {
    if (list)
        for (int i=0; i<list->GetNumChildren(); ++i) {
            auto* row=list->GetChild(i);
            if (row && row->GetAttribute<int>("data-index",-1)==index) return row;
        }
    return nullptr;
}
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
        constexpr int count=25000;
        constexpr int omitted=1;
        SongSelectUi history;
        std::vector<RunResult> runs;
        runs.reserve(count);
        for (int i=0; i<count; ++i) {
            RunResult run;
            run.id=std::to_string(i);
            run.songSetId="test-set";
            run.difficultyId="test-chart";
            run.completedAtMs=1700000000000LL+i;
            runs.push_back(std::move(run));
        }
        history.ReplaceResults({"test-set","test-chart"},std::move(runs));
        auto selected=history.results[omitted];
        selected.hasReplay=true;
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
                    watch->GetOffsetHeight()>0,
                    "no redundant replay status or notes count; watch has a visible action");
        auto* scroll=document->GetElementById("results-history");
        auto* list=document->GetElementById("results-history-list");
        ok &= Check(scroll && list && list->GetNumChildren()<50 &&
                    FindRow(list,0) && !FindRow(list,omitted) && FindRow(list,2),
                    "25k history materializes bounded rows and excludes current attempt");
        if (scroll && list) {
            scroll->SetScrollTop(scroll->GetScrollHeight());
            context->Update();
            presenter.Sync();
            context->Update();
            ok &= Check(FindRow(list,count-1) && list->GetNumChildren()<50,
                        "scrolling reaches last saved attempt without full DOM");
            auto* last=FindRow(list,count-1);
            if (last) last->Click();
            ok &= Check(events.viewResultIndex==count-1,
                        "recycled row emits original model index");
            const float before=presenter.ReadScrollDp(1.6f);
            context->SetDensityIndependentPixelRatio(1.25f);
            context->Update();
            presenter.RestoreScrollDp(before,1.25f);
            presenter.Sync();
            context->Update();
            ok &= Check(FindRow(list,count-1) && list->GetNumChildren()<50,
                        "DPI change retains last history viewport");
        }
        presenter.EndFrame();
        if (auto* last=FindRow(list,count-1)) {
            events.viewResultIndex=-1;
            last->Click();
            ok &= Check(events.viewResultIndex==-1,"no event uses expired frame state");
        }
        presenter.Attach(nullptr);
        document->RemoveEventListener("click",&listener);
        document->Hide();
    }
    if (context) Rml::RemoveContext("results-virtual-test");
    Rml::Shutdown();
    return ok;
}
