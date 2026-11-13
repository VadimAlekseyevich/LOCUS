#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/RenderInterface.h>

#include <cmath>
#include <iostream>
#include <string>

namespace {

struct NullRenderer final : Rml::RenderInterface {
    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f,
                        Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override {
        return 0;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,
                                        Rml::Vector2i) override { return 0; }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

bool Check(bool condition, const char* reason) {
    if (!condition) std::cerr << "RML_LAYOUT_FAIL: " << reason << '\n';
    return condition;
}

} // namespace

int main() {
    NullRenderer renderer;
    Rml::SetRenderInterface(&renderer);
    if (!Rml::Initialise()) return 1;
    bool ok = true;
    auto* context = Rml::CreateContext("headless-layout", {1280, 720});
    if (!context) { Rml::Shutdown(); return 1; }
    const std::string path = "assets/ui/rml/play.rml";
    auto* document = context->LoadDocument(path);
    ok &= Check(document != nullptr, "native PLAY document parses");
    if (document) {
        document->Show();
        context->Update();
        auto* left = document->GetElementById("details");
        auto* right = document->GetElementById("library");
        auto* search = document->GetElementById("search");
        auto* results = document->GetElementById("results-scroll");
        ok &= Check(left && right && search && results,
                    "PLAY has real panels, search and results");
        if (left && right) {
            const float x0 = left->GetAbsoluteLeft();
            const float x1 = right->GetAbsoluteLeft();
            const float w1 = right->GetOffsetWidth();
            ok &= Check(x1 >= x0 + left->GetOffsetWidth() &&
                        x1 + w1 <= 1280.1f && w1 > 400.0f,
                        "1280x720: left/right flex panels have no overlap");

            context->SetDimensions({1920, 810});
            context->Update();
            const float wideRight = right->GetOffsetWidth();
            ok &= Check(wideRight > w1 + 200.0f &&
                        right->GetAbsoluteLeft() +
                            wideRight <= 1920.1f,
                        "ultrawide: list grows with available space");

            context->SetDimensions({800, 540});
            context->Update();
            const float narrow = right->GetOffsetWidth();
            ok &= Check(narrow > 0 &&
                        right->GetAbsoluteLeft() + narrow <= 800.1f,
                        "800-wide window: no horizontal overflow");
        }
        if (search && results)
            ok &= Check(search->GetOffsetHeight() > 20.0f &&
                        results->GetOffsetHeight() >= 0,
                        "search has focusable layout; results can scroll");
    }
    // HOME uses the shared context and adaptive buttons, not fixed hitboxes.
    auto* home = context->LoadDocument("assets/ui/rml/home.rml");
    ok &= Check(home != nullptr, "native HOME document parses");
    if (home) {
        if (document) document->Hide();
        home->Show();
        context->SetDimensions({960, 540});
        context->Update();
        auto* actions = home->GetElementById("home-actions");
        auto* play = home->GetElementById("home-play");
        auto* editor = home->GetElementById("home-editor");
        auto* exit = home->GetElementById("home-exit");
        ok &= Check(actions && play && editor && exit,
                    "HOME has real navigation controls");
        if (play && editor && exit) {
            auto* first = home->FindNextTabElement(home, true);
            auto* second = first ? home->FindNextTabElement(first, true)
                                 : nullptr;
            ok &= Check(first == play && second == editor,
                        "HOME keyboard Tab reaches PLAY then EDITOR");
            ok &= Check(play->GetOffsetWidth() > 100.0f &&
                        editor->GetAbsoluteLeft() >=
                            play->GetAbsoluteLeft() + play->GetOffsetWidth() &&
                        exit->GetAbsoluteLeft() + exit->GetOffsetWidth() <= 960.1f,
                        "960x540: HOME actions fit");
            context->SetDimensions({800, 450});
            context->Update();
            ok &= Check(editor->GetAbsoluteLeft() >=
                            play->GetAbsoluteLeft() + play->GetOffsetWidth() &&
                        exit->GetAbsoluteLeft() + exit->GetOffsetWidth() <= 800.1f,
                        "800x450: HOME actions fit");
            context->SetDimensions({1920, 810});
            context->Update();
            ok &= Check(actions->GetOffsetWidth() > 0 &&
                        exit->GetAbsoluteLeft() + exit->GetOffsetWidth() <= 1920.1f,
                        "1920x810: HOME actions fit");
        }
    }
    Rml::RemoveContext("headless-layout");
    Rml::Shutdown();
    return ok ? 0 : 1;
}
