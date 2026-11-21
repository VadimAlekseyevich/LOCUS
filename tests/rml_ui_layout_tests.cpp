#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/StyleTypes.h>
#include "app/rml_ui_text.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

struct NullRenderer final : Rml::RenderInterface {
    std::size_t texturedDraws = 0;
    Rml::TextureHandle nextTexture = 1;
    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f,
                        Rml::TextureHandle texture) override {
        if (texture != 0) ++texturedDraws;
    }
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override {
        return 0;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,
                                        Rml::Vector2i) override {
        return nextTexture++;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};


bool TestFontAvailable() {
#ifdef _WIN32
    const std::array<const char*, 2> fonts = {
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"
    };
#else
    const std::array<const char*, 2> fonts = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"
    };
#endif
    for (const char* path : fonts) {
        if (!std::filesystem::exists(path) ||
            !Rml::LoadFontFace(path, "Locus",
                Rml::Style::FontStyle::Normal,
                Rml::Style::FontWeight::Normal))
            continue;
        // The production adapter also registers the bold face. HOME is
        // made entirely of heavyweight button labels, unlike PLAY which
        // includes normal-weight body text. A regular-only test font setup
        // would fail to render HOME despite a working production renderer.
        const std::filesystem::path regular(path);
        const std::filesystem::path bold = regular.parent_path() /
            (regular.filename() == "segoeui.ttf" ? "segoeuib.ttf" :
             regular.filename() == "arial.ttf" ? "arialbd.ttf" :
             regular.filename() == "DejaVuSans.ttf" ? "DejaVuSans-Bold.ttf" :
             "LiberationSans-Bold.ttf");
        if (std::filesystem::exists(bold))
            Rml::LoadFontFace(bold.u8string(), "Locus",
                Rml::Style::FontStyle::Normal,
                Rml::Style::FontWeight::Bold);
        return true;
    }
    return false;
}

bool HasButtonLabels(Rml::Element* root) {
    if (!root) return true;
    if (root->GetTagName() == "button") {
        bool wrapped = false;
        for (int i = 0; i < root->GetNumChildren(); ++i) {
            if (auto* child = root->GetChild(i);
                child && child->GetTagName() == "span" &&
                child->IsClassSet("button-label")) {
                wrapped = true;
                break;
            }
        }
        if (!wrapped) {
            std::cerr << "RML_LAYOUT_FAIL: button missing text wrapper: "
                      << root->GetId() << '\n';
            return false;
        }
    }
    for (int i = 0; i < root->GetNumChildren(); ++i)
        if (!HasButtonLabels(root->GetChild(i))) return false;
    return true;
}

bool Check(bool condition, const char* reason) {
    if (!condition) std::cerr << "RML_LAYOUT_FAIL: " << reason << '\n';
    return condition;
}

// Visibility alone is insufficient: a caption can render at the bottom
// border even while every element has a valid positive layout rectangle.
// Check RmlUi's final physical line-box geometry, not source CSS strings.
bool LabelCentered(Rml::Element* button, float tolerance = 3.0f) {
    if (!button) return false;
    Rml::Element* label = nullptr;
    for (int i = 0; i < button->GetNumChildren(); ++i) {
        auto* child = button->GetChild(i);
        if (child && child->IsClassSet("button-label")) {
            label = child;
            break;
        }
    }
    if (!label) return false;
    const float labelCenter = label->GetAbsoluteTop() + 
                              label->GetOffsetHeight() * 0.5f;
    const float buttonCenter = button->GetAbsoluteTop() +
                               button->GetOffsetHeight() * 0.5f;
    if (std::abs(labelCenter - buttonCenter) > tolerance) {
        std::cerr << "RML_LAYOUT_FAIL: off-center label on "
                  << button->GetId() << " (delta="
                  << (labelCenter - buttonCenter) << ")\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    NullRenderer renderer;
    Rml::SetRenderInterface(&renderer);
    if (!Rml::Initialise()) return 1;
    bool ok = true;
    // A valid CSS rectangle cannot prove readable UI. Load a real font and
    // require actual textured glyph geometry from RmlUi's render stage.
    ok &= Check(TestFontAvailable(), "RmlUi can load an installed font face");
    auto* context = Rml::CreateContext("headless-layout", {1280, 720});
    if (!context) { Rml::Shutdown(); return 1; }
    const std::string path = "assets/ui/rml/play.rml";
    auto* document = context->LoadDocument(path);
    ok &= Check(document != nullptr, "native PLAY document parses");
    if (document) {
        document->Show();
        context->Update();
        ok &= Check(HasButtonLabels(document),
                    "PLAY buttons contain explicit text elements");
        renderer.texturedDraws = 0;
        context->Render();
        ok &= Check(renderer.texturedDraws > 0,
                    "PLAY font glyphs produce textured render geometry");
        auto* left = document->GetElementById("details");

        auto* right = document->GetElementById("library");
        auto* search = document->GetElementById("search");
        auto* results = document->GetElementById("results-scroll");
        auto* metric = document->GetElementById("metric-speed");
        auto* setButton = document->GetElementById("set-button");
        ok &= Check(left && right && search && results && metric && setButton,
                    "PLAY has separate summary, metrics and real action controls");
        // RmlUi supplies no user-agent CSS. Without explicit display:block
        // on divs, summary values form a single overlapping inline row.
        for (const char* id : {"song-title", "song-author",
                               "song-meta", "chart-title", "chart-notes"}) {
            auto* element = document->GetElementById(id);
            ok &= Check(element && element->GetDisplay() ==
                                      Rml::Style::Display::Block,
                        "PLAY metadata fields must have block layout");
        }
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
        auto* wordmark = home->GetElementById("home-wordmark");
        ok &= Check(HasButtonLabels(home),
                    "HOME buttons contain explicit text elements");
        renderer.texturedDraws = 0;
        context->Render();
        ok &= Check(renderer.texturedDraws > 0,
                    "HOME button labels produce textured glyph geometry");

        ok &= Check(wordmark != nullptr, "HOME wordmark is present");
        if (wordmark) {
            ok &= Check(wordmark->GetTagName() == "div",
                        "HOME logo is raylib-backed, not an unloaded RmlUi image");
            std::ifstream image("assets/ui/wordmark.png", std::ios::binary);
            std::array<unsigned char, 24> header{};
            image.read(reinterpret_cast<char*>(header.data()), header.size());
            constexpr std::array<unsigned char, 8> png = {
                0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
            const auto dimension = [&](int i) {
                return (static_cast<unsigned int>(header[i]) << 24) |
                       (static_cast<unsigned int>(header[i + 1]) << 16) |
                       (static_cast<unsigned int>(header[i + 2]) << 8) |
                       static_cast<unsigned int>(header[i + 3]);
            };
            ok &= Check(image.gcount() == static_cast<std::streamsize>(header.size()) &&
                        std::equal(png.begin(), png.end(), header.begin()) &&
                        dimension(16) == 460 && dimension(20) == 136,
                        "Approved 460x136 PNG wordmark exists and is intact");
        }
        auto* actions = home->GetElementById("home-actions");
        auto* play = home->GetElementById("home-play");
        auto* editor = home->GetElementById("home-editor");
        auto* exit = home->GetElementById("home-exit");
        ok &= Check(actions && play && editor && exit,
                    "HOME has real navigation controls");
        if (play && editor && exit && wordmark) {
            ok &= Check(LabelCentered(play) && LabelCentered(editor) &&
                        LabelCentered(exit),
                        "HOME button captions are vertically centered");
            // Direct text updates must preserve the button's child node and
            // focusable identity, even when captions contain markup characters.
            auto* label = play->GetChild(0);
            locus::app::UpdateRmlText(play, "PLAY & TEST");
            ok &= Check(play->GetChild(0) == label &&
                        label->IsClassSet("button-label"),
                        "RmlUi text update preserves button DOM identity");
            locus::app::UpdateRmlText(play, "PLAY");
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
            context->SetDimensions({1536, 960});
            context->SetDensityIndependentPixelRatio(1.6f);
            context->Update();
            ok &= Check(play->GetOffsetHeight() >= 120.0f &&
                        wordmark->GetOffsetWidth() >= 550.0f &&
                        wordmark->GetAbsoluteTop() + wordmark->GetOffsetHeight() <=
                            play->GetAbsoluteTop() - 70.0f,
                        "1536x960: Nightbloom controls and wordmark scale together");
            context->SetDimensions({1920, 810});
            context->SetDensityIndependentPixelRatio(1.5f);
            context->Update();
            ok &= Check(actions->GetOffsetWidth() > 0 &&
                        exit->GetAbsoluteLeft() + exit->GetOffsetWidth() <= 1920.1f,
                        "1920x810: HOME actions fit");
        }
    }
    // SETTINGS shares the same context, with independent scrolling bindings.
    auto* settingsDoc = context->LoadDocument("assets/ui/rml/settings.rml");
    ok &= Check(settingsDoc != nullptr, "native SETTINGS document parses");
    if (settingsDoc) {
        if (home) home->Hide();
        settingsDoc->Show();
        context->SetDimensions({960, 540});
        context->SetDensityIndependentPixelRatio(1.f);
        context->Update();
        auto* left = settingsDoc->GetElementById("settings-display");
        ok &= Check(HasButtonLabels(settingsDoc),
                    "SETTINGS buttons contain explicit text elements");
        renderer.texturedDraws = 0;
        context->Render();
        ok &= Check(renderer.texturedDraws > 0,
                    "SETTINGS font glyphs produce textured render geometry");

        auto* right = settingsDoc->GetElementById("settings-bindings");
        auto* bindings = settingsDoc->GetElementById("bind-scroll");
        auto* modal = settingsDoc->GetElementById("settings-calibration");
        auto* volume = settingsDoc->GetElementById("value-volume");
        ok &= Check(left && right && bindings && modal && volume,
                    "SETTINGS contains Display, Audio, bindings and calibration");
        // Reproduce the runtime-generated binding markup: its buttons
        // and action labels must both remain visible and the row must
        // expand to the available right panel width.
        if (auto* list = settingsDoc->GetElementById("bind-list")) {
            list->SetInnerRML(
                "<div class='binding-row' id='test-binding-row'>"
                "<div class='binding-label'>Pause / Menu</div>"
                "<button class='binding-key'>"
                "<span class='button-label'>ESC</span></button></div>");
            context->Update();
            auto* bindingRow = settingsDoc->GetElementById("test-binding-row");
            if (bindingRow && right)
                ok &= Check(bindingRow->GetOffsetWidth() >
                            right->GetOffsetWidth() * 0.65f,
                            "SETTINGS binding rows fill the scroll panel");
            else
                ok &= Check(false, "SETTINGS dynamic binding row parses");
            ok &= Check(HasButtonLabels(list),
                        "SETTINGS generated binding buttons have labels");
            if (auto* label = settingsDoc->GetElementById("test-binding-row")) {
                auto* child = label->GetChild(0);
                ok &= Check(child && child->GetDisplay() == Rml::Style::Display::Block &&
                            child->GetOffsetWidth() > 30.0f,
                            "SETTINGS binding name is a block flex item with nonzero width");
            }
        }
        if (left && right) {
            ok &= Check(right->GetAbsoluteLeft() > left->GetAbsoluteLeft() &&
                        right->GetAbsoluteLeft() +
                            right->GetOffsetWidth() <= 960.1f,
                        "960x540: SETTINGS columns fit side by side");
            context->SetDimensions({1536, 960});
            context->SetDensityIndependentPixelRatio(1.6f);
            context->Update();
            ok &= Check(right->GetAbsoluteLeft() +
                            right->GetOffsetWidth() <= 1536.1f,
                        "1536x960: SETTINGS scales without overflow");
        }
    }
    // Editor shell follows the 960x540 raylib viewport exactly.
    auto* editorDoc = context->LoadDocument("assets/ui/rml/editor.rml");
    ok &= Check(editorDoc != nullptr, "EDITOR shell parses");
    if (editorDoc) {
        if (settingsDoc) settingsDoc->Hide();
        editorDoc->Show();
        context->SetDimensions({960, 540});
        context->SetDensityIndependentPixelRatio(1.0f);
        context->Update();
        auto* canvas = editorDoc->GetElementById("editor-canvas");
        for (const char* id : {"editor-back", "editor-tab-route",
                               "editor-tool-mirror-h", "editor-tool-mirror-v",
                               "editor-tool-rotate-ccw", "editor-tool-rotate-cw",
                               "editor-tool-test", "editor-pad-7",
                               "editor-pad-0", "editor-pad-1",
                               "editor-pad-4", "editor-preview-play"}) {
            ok &= Check(LabelCentered(editorDoc->GetElementById(id)),
                        "EDITOR label centered inside its RmlUi button");
        }
        for (const char* id : {"editor-tool-mirror-h", "editor-tool-mirror-v",
                               "editor-tool-rotate-ccw", "editor-tool-rotate-cw",
                               "editor-tool-test", "editor-pad-7", "editor-pad-0",
                               "editor-pad-4", "editor-pad-3", "editor-pad-1",
                               "editor-pad-6", "editor-pad-2", "editor-pad-5"}) {
            const auto* icon = editorDoc->GetElementById(id);
            ok &= Check(icon != nullptr, "vector glyph button exists");
        }
        ok &= Check(HasButtonLabels(editorDoc),
                    "EDITOR buttons contain explicit text elements");
        renderer.texturedDraws = 0;
        context->Render();
        ok &= Check(renderer.texturedDraws > 0,
                    "EDITOR header and transport labels produce glyph geometry");

        auto* header = editorDoc->GetElementById("editor-header");
        auto* set = editorDoc->GetElementById("editor-tab-set");
        auto* route = editorDoc->GetElementById("editor-tab-route");
        auto* audio = editorDoc->GetElementById("editor-tab-audio");
        auto* footer = editorDoc->GetElementById("editor-footer-playback");
        auto* rate = editorDoc->GetElementById("editor-preview-rate");
        ok &= Check(rate && rate->GetTagName() == "input" &&
                    rate->GetAttribute<Rml::String>("maxlength", "") == "3",
                    "EDITOR rate is a numeric-entry input, not a dropdown button");
        if (auto* control = dynamic_cast<Rml::ElementFormControlInput*>(rate)) {
            // Real RmlUi text editing must support replacing the value in one
            // gesture and a temporarily empty field (unlike the old immediate
            // clamp-on-change behavior).
            const auto* parent = control->GetParentNode();
            ok &= Check(parent && std::abs(
                            control->GetAbsoluteLeft() +
                            control->GetOffsetWidth() * 0.5f -
                            (parent->GetAbsoluteLeft() +
                             parent->GetOffsetWidth() * 0.5f)) < 2.0f,
                        "EDITOR rate field is centered horizontally");
            ok &= Check(control->Focus(), "EDITOR rate input accepts focus");
            control->Select();
            int first = -1, last = -1;
            Rml::String selection;
            control->GetSelection(&first, &last, &selection);
            ok &= Check(first == 0 && last == 3 && selection == "100",
                        "EDITOR rate can select the entire initial value");
            control->SetValue("");
            ok &= Check(control->GetValue().empty(),
                        "EDITOR rate permits an empty intermediate value");
            control->SetValue("50");
            ok &= Check(control->GetValue() == "50",
                        "EDITOR rate accepts replacement without retained digits");
            control->Blur();
        }
        auto* time = editorDoc->GetElementById("editor-preview-time");
        auto* compass = editorDoc->GetElementById("editor-route-pad");
        auto* toolrail = editorDoc->GetElementById("editor-route-tools");
        ok &= Check(canvas && header && set && route && audio &&
                    footer && rate && time && compass && toolrail,
                    "EDITOR has native tabs and transport controls");
        if (header && footer && canvas) {
            ok &= Check(compass && toolrail &&
                compass->GetOffsetWidth() > 170.0f &&
                toolrail->GetAbsoluteLeft() + toolrail->GetOffsetWidth() <
                    compass->GetAbsoluteLeft(),
                "EDITOR ROUTE native compass and toolrail are separate");
            ok &= Check(std::abs(header->GetAbsoluteTop() - 65.0f) < 2.0f &&
                        std::abs(footer->GetAbsoluteTop() - 485.0f) < 2.0f,
                        "960x540: editor shell aligns with raylib viewport");
            context->SetDimensions({1536, 960});
            context->SetDensityIndependentPixelRatio(1.6f);
            canvas->SetProperty("top", "30dp");
            context->Update();
            ok &= Check(std::abs(header->GetAbsoluteTop() - 152.0f) < 4.0f &&
                        footer->GetAbsoluteTop() < 960.0f,
                        "1536x960: editor shell respects letterboxing");
        }
    }
    auto* resultsDoc = context->LoadDocument("assets/ui/rml/results.rml");
    ok &= Check(resultsDoc != nullptr, "native RESULTS document parses");
    if (resultsDoc) {
        if (editorDoc) editorDoc->Hide();
        resultsDoc->Show();
        context->SetDimensions({960, 540});
        context->SetDensityIndependentPixelRatio(1.0f);
        context->Update();
        ok &= Check(HasButtonLabels(resultsDoc), "RESULTS buttons contain labels");
        auto* accuracy = resultsDoc->GetElementById("results-accuracy");
        auto* history = resultsDoc->GetElementById("results-history");
        auto* retry = resultsDoc->GetElementById("results-retry");
        ok &= Check(accuracy && history && retry && accuracy->GetOffsetWidth() > 0 &&
                    history->GetOffsetHeight() > 0,
                    "RESULTS has separate score, history and action regions");
        context->SetDimensions({800, 450});
        context->Update();
        ok &= Check(retry && retry->GetAbsoluteLeft() + retry->GetOffsetWidth()
                    <= 800.1f, "800x450 RESULTS actions fit");
    }
    auto* skinDoc = context->LoadDocument("assets/ui/rml/skins.rml");
    ok &= Check(skinDoc != nullptr, "native SKINS document parses");
    if (skinDoc) {
        if (resultsDoc) resultsDoc->Hide();
        skinDoc->Show();
        context->SetDimensions({960,540});
        context->SetDensityIndependentPixelRatio(1.0f);
        context->Update();
        auto* controls = skinDoc->GetElementById("skin-controls");
        auto* preview = skinDoc->GetElementById("skin-preview-canvas");
        auto* name = skinDoc->GetElementById("skin-name");
        ok &= Check(HasButtonLabels(skinDoc) && controls && preview && name &&
                    preview->GetOffsetWidth() > 120.0f &&
                    preview->GetOffsetHeight() > 70.0f,
                    "SKINS contains responsive authoring panel and live preview");
        context->SetDimensions({800,450});
        context->Update();
        ok &= Check(preview && preview->GetAbsoluteLeft() + preview->GetOffsetWidth()
                    <= 800.1f, "800x450 SKINS preview fits");
    }
    Rml::RemoveContext("headless-layout");
    Rml::Shutdown();
    return ok ? 0 : 1;
}
