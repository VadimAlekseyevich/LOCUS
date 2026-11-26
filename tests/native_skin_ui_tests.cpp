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
#include <cmath>
#include "app/ui_scale.h"

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
// WidgetTextInput positions its first line at the content-box origin.
// Test the final layout at actual display density, not the style source.
bool CenteredInput(Rml::Element* field) {
    if (!field) return false;
    const auto& box = field->GetBox();
    const float textCenter = box.GetPosition(Rml::BoxArea::Content).y +
        std::round(field->GetLineHeight()) * 0.5f;
    const float fieldCenter = box.GetPosition(Rml::BoxArea::Border).y +
        box.GetSize(Rml::BoxArea::Border).y * 0.5f;
    const float delta = textCenter-fieldCenter;
    if (std::abs(delta)>1.5f) {
        std::cerr << "NATIVE_SKIN_FAIL: off-center input " << field->GetId()
                  << " (delta=" << delta << ")\n";
        return false;
    }
    return box.GetSize(Rml::BoxArea::Content).y+1.5f>=field->GetLineHeight();
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
        auto* hitError=document->GetElementById("skin-tab-hit-error");
        auto* nameRow=document->GetElementById("skin-name-row");
        auto* hexRow=document->GetElementById("skin-color-edit");
        ok &= Check(ui.page==3 && profiles && profiles->IsClassSet("selected") &&
                    path && profiles->GetAbsoluteLeft()<path->GetAbsoluteLeft() &&
                    name && preview && colors && player && hitError && nameRow && hexRow,
                    "SKINS is the initial first tab with rename in left panel");
        if (name && preview && profiles && path && colors && player && hitError && nameRow && hexRow) {
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
            ok &= Check(CenteredInput(number),"PATH number line is centered");
            Type(*context,*number,"1200");
            context->Update();
            ok &= Check(CenteredInput(number),"focus preserves vertical centering");
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
            for (const auto size : {Rml::Vector2i{1280,720},Rml::Vector2i{800,450},
                                    Rml::Vector2i{1920,1200}}) {
                context->SetDimensions(size);
                context->SetDensityIndependentPixelRatio(
                    NativeUiDensity(size.x,size.y,1.0f));
                profiles->Click();
                presenter.Sync();
                context->Update();
                const float x=preview->GetAbsoluteLeft(),y=preview->GetAbsoluteTop();
                const auto width=preview->GetOffsetWidth(),height=preview->GetOffsetHeight();
                for (auto* tab : {path,player,hitError,colors,profiles}) {
                    tab->Click();
                    presenter.Sync();
                    context->Update();
                    ok &= Check(preview->GetAbsoluteLeft()==x && preview->GetAbsoluteTop()==y &&
                                preview->GetOffsetWidth()==width && preview->GetOffsetHeight()==height,
                                "preview geometry identical across every skin tab");
                    if (SkinSettingCount(ui.page)>0) {
                        for (int row=0;row<SkinSettingCount(ui.page);++row) {
                            const auto* setting=GetSkinSetting(ui.page,row);
                            if (!setting || !setting->number) continue;
                            auto* field=document->GetElementById("skin-number-"+std::to_string(row));
                            ok &= Check(CenteredInput(field),"numeric line centered at native UI density");
                        }
                    } else if (ui.page==3) {
                        ok &= Check(CenteredInput(name),"skin name centered");
                    }
                    if (ui.page==2) {
                        auto* hex=document->GetElementById("skin-hex");
                        auto* wheel=document->GetElementById("skin-wheel-canvas");
                        ok &= Check(CenteredInput(hex),"HEX line centered");
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
