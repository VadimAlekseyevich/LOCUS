#include "app/native_skin_ui.h"
#include "app/menu_controller.h"
#include "app/rml_native_files.h"
#include "app/skin_controls.h"
#include "rml_test_support.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <iostream>

namespace {
using namespace locus::app;
class SkinEvents final : public Rml::EventListener {
public:
    explicit SkinEvents(NativeSkinUi& presenter) : presenter_(presenter) {}
    void ProcessEvent(Rml::Event& event) override { presenter_.ProcessEvent(event); }
private:
    NativeSkinUi& presenter_;
};
bool Check(bool ok,const char* label) {
    if (!ok) std::cerr << "NATIVE_SKIN_FAIL: " << label << '\n';
    return ok;
}
void Type(Rml::Context& context,Rml::ElementFormControlInput& field,const std::string& value) {
    field.Focus();
    field.Select();
    for (unsigned char c : value)
        context.ProcessTextInput(static_cast<Rml::Character>(c));
}
} // namespace

bool TestNativeSkinTextEditing() {
    using namespace locus::app;
    RmlNativeFiles files;
    locus::tests::NullRenderer renderer;
    Rml::SetFileInterface(&files);
    Rml::SetRenderInterface(&renderer);
    if (!Check(Rml::Initialise(),"RmlUi initializes")) return false;
    bool ok = Check(locus::tests::TestFontAvailable(),"font loads");
    auto* context = Rml::CreateContext("skin-presenter-test",{1280,720});
    auto* document = context ? context->LoadDocument("assets/ui/rml/skins.rml") : nullptr;
    ok &= Check(document != nullptr,"production SKINS document loads");
    if (document) {
        NativeSkinUi presenter;
        SkinEvents listener(presenter);
        document->AddEventListener("click",&listener);
        document->AddEventListener("focus",&listener,true);
        document->AddEventListener("blur",&listener,true);
        Skin skin;
        SkinUi ui;
        FrameInput input;
        MenuEvents events;
        presenter.Attach(document);
        presenter.BeginFrame(skin,ui,input,events);
        document->Show();
        presenter.Sync();
        context->Update();
        auto* name = dynamic_cast<Rml::ElementFormControlInput*>(document->GetElementById("skin-name"));
        auto* preview=document->GetElementById("skin-preview-canvas");
        auto* profiles=document->GetElementById("skin-tab-profiles");
        auto* path=document->GetElementById("skin-tab-path");
        auto* colors=document->GetElementById("skin-tab-colors");
        auto* player=document->GetElementById("skin-tab-player");
        auto* nameRow=document->GetElementById("skin-name-row");
        auto* hexRow=document->GetElementById("skin-color-edit");
        ok &= Check(ui.page==3 && profiles && profiles->IsClassSet("selected") &&
                    path && profiles->GetAbsoluteLeft()<path->GetAbsoluteLeft() &&
                    name && preview && colors && player && nameRow && hexRow,
                    "SKINS is the initial first tab with rename in left panel");
        if (name && preview && profiles && path && colors && player && nameRow && hexRow) {
            const std::string originalName = skin.name;
            const double originalNumber = GetSkinNumber(skin,0,0,ui.selectedColor);
            Type(*context,*name,"New Skin");
            ok &= Check(name->GetValue()=="New Skin" && skin.name==originalName && !ui.dirty,
                        "typing a name changes only the edit buffer");
            presenter.FinishFocusedText(false);
            presenter.Sync();
            context->Update();
            ok &= Check(name->GetValue()==originalName && skin.name==originalName && !ui.dirty,
                        "Esc cancels name without dirtying skin");
            Type(*context,*name,"Renamed Skin");
            path->Click();
            presenter.Sync();
            context->Update();
            ok &= Check(skin.name=="Renamed Skin" && ui.dirty && ui.page==0,
                        "switching tab commits valid name");
            ui.dirty=false;
            auto* number = dynamic_cast<Rml::ElementFormControlInput*>(document->GetElementById("skin-number-0"));
            if (!Check(number!=nullptr,"PATH numeric input exists")) {
                presenter.EndFrame();
                presenter.Attach(nullptr);
                document->RemoveEventListener("click",&listener);
                document->RemoveEventListener("focus",&listener,true);
                document->RemoveEventListener("blur",&listener,true);
                Rml::RemoveContext("skin-presenter-test");
                Rml::Shutdown();
                return false;
            }
            Type(*context,*number,"1200");
            ok &= Check(number->GetValue()=="1200" &&
                        GetSkinNumber(skin,0,0,ui.selectedColor)==originalNumber && !ui.dirty,
                        "typing a number cannot mutate model until commit");
            presenter.FinishFocusedText(true);
            presenter.Sync();
            context->Update();
            ok &= Check(GetSkinNumber(skin,0,0,ui.selectedColor)==1200 && ui.dirty,
                        "Enter commits numeric value once");
            ui.dirty = false;
            Type(*context,*number,"bad");
            presenter.FinishFocusedText(true);
            presenter.Sync();
            context->Update();
            ok &= Check(GetSkinNumber(skin,0,0,ui.selectedColor)==1200 &&
                        number->GetValue()==FormatSkinNumber(1200) && !ui.dirty && ui.noticeError,
                        "invalid blur restores previous value without mutating skin");
            // The shared preview is independent of all left-panel fields.
            for (const auto size : {Rml::Vector2i{1280,720},Rml::Vector2i{800,450}}) {
                context->SetDimensions(size);
                profiles->Click();
                presenter.Sync();
                context->Update();
                const float x=preview->GetAbsoluteLeft(),y=preview->GetAbsoluteTop();
                const auto width=preview->GetOffsetWidth(),height=preview->GetOffsetHeight();
                for (auto* tab : {path,player,colors,profiles}) {
                    tab->Click();
                    presenter.Sync();
                    context->Update();
                    ok &= Check(preview->GetAbsoluteLeft()==x && preview->GetAbsoluteTop()==y &&
                                preview->GetOffsetWidth()==width && preview->GetOffsetHeight()==height,
                                "preview geometry identical across every skin tab");
                    if (ui.page==2) {
                        auto* hex=document->GetElementById("skin-hex");
                        auto* wheel=document->GetElementById("skin-wheel-canvas");
                        ok &= Check(hex && wheel && hex->GetAbsoluteTop()>=
                                    wheel->GetAbsoluteTop()+wheel->GetOffsetHeight()-1,
                                    "hex input follows color wheel in left panel");
                    }
                }
            }
        }
        presenter.EndFrame();
        presenter.Attach(nullptr);
        document->RemoveEventListener("click",&listener);
        document->RemoveEventListener("focus",&listener,true);
        document->RemoveEventListener("blur",&listener,true);
        document->Hide();
    }
    if (context) Rml::RemoveContext("skin-presenter-test");
    Rml::Shutdown();
    return ok;
}
