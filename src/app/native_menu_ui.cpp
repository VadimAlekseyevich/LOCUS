#include "app/native_menu_ui.h"
#include "app/executable_directory.h"

#include "app/menu_ui.h"
#include "app/viewport.h"
#include "app/rml_ui_text.h"
#include "app/rml_layout_geometry.h"
#include "app/rml_modal_focus.h"
#include "app/results_keyboard_input.h"
#include "app/skin_controls.h"
#include "core/utf8.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StyleTypes.h>

#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace locus::app {
namespace {

int Modifiers() {
    int result = 0;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
        result |= Rml::Input::KM_SHIFT;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
        result |= Rml::Input::KM_CTRL;
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))
        result |= Rml::Input::KM_ALT;
    return result;
}

Rml::Input::KeyIdentifier ToRmlKey(int raylibKey) {
    using namespace Rml::Input;
    if (raylibKey >= KEY_A && raylibKey <= KEY_Z)
        return static_cast<KeyIdentifier>(KI_A + raylibKey - KEY_A);
    if (raylibKey >= KEY_ZERO && raylibKey <= KEY_NINE)
        return static_cast<KeyIdentifier>(KI_0 + raylibKey - KEY_ZERO);
    switch (raylibKey) {
        case KEY_BACKSPACE: return KI_BACK;
        case KEY_DELETE: return KI_DELETE;
        case KEY_ENTER: return KI_RETURN;
        case KEY_KP_ENTER: return KI_NUMPADENTER;
        case KEY_TAB: return KI_TAB;
        case KEY_SPACE: return KI_SPACE;
        case KEY_ESCAPE: return KI_ESCAPE;
        case KEY_LEFT: return KI_LEFT;
        case KEY_RIGHT: return KI_RIGHT;
        case KEY_UP: return KI_UP;
        case KEY_DOWN: return KI_DOWN;
        case KEY_HOME: return KI_HOME;
        case KEY_END: return KI_END;
        case KEY_PAGE_UP: return KI_PRIOR;
        case KEY_PAGE_DOWN: return KI_NEXT;
        case KEY_LEFT_SHIFT: return KI_LSHIFT;
        case KEY_RIGHT_SHIFT: return KI_RSHIFT;
        case KEY_LEFT_CONTROL: return KI_LCONTROL;
        case KEY_RIGHT_CONTROL: return KI_RCONTROL;
        case KEY_LEFT_ALT: return KI_LMENU;
        case KEY_RIGHT_ALT: return KI_RMENU;
        default: return KI_UNKNOWN;
    }
}
constexpr int kUiKeys[] = {
    KEY_BACKSPACE, KEY_DELETE, KEY_ENTER, KEY_KP_ENTER,
    KEY_TAB, KEY_SPACE, KEY_ESCAPE,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_HOME, KEY_END,
    KEY_PAGE_UP, KEY_PAGE_DOWN,
    KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT,
    KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL,
    KEY_LEFT_ALT, KEY_RIGHT_ALT
};

bool LoadUiFontFace() {
    namespace fs = std::filesystem;
    const char* preferred[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
    };
    std::string regularPath;
    for (const char* path : preferred) {
        if (!fs::exists(fs::u8path(path))) continue;
        if (!Rml::LoadFontFace(path, "Locus", Rml::Style::FontStyle::Normal,
                               Rml::Style::FontWeight::Normal)) continue;
        regularPath = path;
        const fs::path regular = fs::u8path(path);
        const fs::path bold = regular.parent_path() /
            (regular.filename() == "segoeui.ttf" ? "segoeuib.ttf" :
             regular.filename() == "arial.ttf" ? "arialbd.ttf" :
             regular.filename() == "DejaVuSans.ttf" ? "DejaVuSans-Bold.ttf" :
             "LiberationSans-Bold.ttf");
        if (fs::exists(bold))
            Rml::LoadFontFace(bold.u8string(), "Locus",
                Rml::Style::FontStyle::Normal,
                Rml::Style::FontWeight::Bold);
        break;
    }
    if (regularPath.empty()) return false;

    // UTF-8 is not sufficient by itself: the selected font must contain
    // the Unicode arrows/rotation glyphs. The icon family is deliberately
    // separate from ordinary UI typography.
    const char* symbolFonts[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/seguisym.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
    };
    bool iconFontLoaded = false;
    for (const char* path : symbolFonts) {
        if (fs::exists(fs::u8path(path)) &&
            Rml::LoadFontFace(path, "LocusIcons",
                Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, true)) {
            iconFontLoaded = true;
            break;
        }
    }
    if (!iconFontLoaded)
        Rml::LoadFontFace(regularPath, "LocusIcons",
            Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal);
    return true;
}

} // namespace

NativeMenuUi::~NativeMenuUi() { Shutdown(); }

bool NativeMenuUi::Initialize() {
    if (Available()) return true;
    if (GetScreenWidth() <= 0 || GetScreenHeight() <= 0) return false;
    Rml::SetFileInterface(&files_);
    Rml::SetSystemInterface(&system_);
    Rml::SetRenderInterface(&renderer_);
    if (!Rml::Initialise()) return false;
    const bool fontLoaded = LoadUiFontFace();
    if (!fontLoaded) {
        std::cerr << "RmlUi font unavailable; using legacy PLAY UI.\n";
        Rml::Shutdown();
        return false;
    }
    context_ = Rml::CreateContext("locus-menu", {
        GetScreenWidth(), GetScreenHeight()});
    if (!context_) {
        Rml::Shutdown();
        return false;
    }
    const std::filesystem::path file =
        ExecutableDirectory() /
        "assets" / "ui" / "rml" / "play.rml";
    document_ = context_->LoadDocument(file.generic_u8string());
    if (!document_) {
        std::cerr << "Could not load PLAY RmlUi file: " << file.generic_u8string() << '\n';
        Rml::RemoveContext("locus-menu");
        context_ = nullptr;
        Rml::Shutdown();
        return false;
    }
    // Home is optional: a bad document must not disable working PLAY.
    homeDocument_ = context_->LoadDocument(
        (file.parent_path() / "home.rml").generic_u8string());
    if (homeDocument_) {
        homeDocument_->AddEventListener("click", this);
        homeDocument_->Hide();
    } else {
        std::cerr << "Could not load HOME RmlUi; using legacy HOME UI.\n";
    }
    settingsDocument_ = context_->LoadDocument(
        (file.parent_path() / "settings.rml").generic_u8string());
    if (settingsDocument_) {
        settingsDocument_->AddEventListener("click", this);
        settingsDocument_->Hide();
        settingsPresenter_.Attach(settingsDocument_);
    } else {
        std::cerr << "Could not load SETTINGS RmlUi; using legacy settings.\n";
    }
    editorDocument_ = context_->LoadDocument(
        (file.parent_path() / "editor.rml").generic_u8string());
    if (editorDocument_) {
        editorDocument_->AddEventListener("click", this);
        editorDocument_->AddEventListener("change", this);
        // Capture focus events for all inputs, including dynamic tempo rows.
        editorDocument_->AddEventListener("focus", this, true);
        editorDocument_->AddEventListener("blur", this, true);
        editorDocument_->Hide();
        editorPresenter_.Attach(editorDocument_);
    } else {
        std::cerr << "Could not load editor chrome; using raylib editor UI.\n";
    }
    resultsDocument_ = context_->LoadDocument(
        (file.parent_path() / "results.rml").generic_u8string());
    if (resultsDocument_) {
        resultsDocument_->AddEventListener("click", this);
        resultsDocument_->Hide();
        resultsPresenter_.Attach(resultsDocument_);
    } else {
        std::cerr << "Could not load RESULTS RmlUi; using raylib results UI.\n";
    }
    skinDocument_ = context_->LoadDocument(
        (file.parent_path() / "skins.rml").generic_u8string());
    if (skinDocument_) {
        skinDocument_->AddEventListener("click",this);
        skinDocument_->AddEventListener("focus",this,true);
        skinDocument_->AddEventListener("blur",this,true);
        skinDocument_->Hide();
        skinPresenter_.Attach(skinDocument_);
    } else {
        std::cerr << "Could not load SKINS RmlUi; using raylib SKINS.\n";
    }
    // Global prompts must render above gameplay and native custom canvases,
    // and cannot deliver events to a control in the underlying document.
    confirmationContext_ = Rml::CreateContext("locus-confirmation",
        {GetScreenWidth(), GetScreenHeight()});
    if (confirmationContext_) {
        confirmationDocument_ = confirmationContext_->LoadDocument(
            (file.parent_path() / "confirmation.rml").generic_u8string());
        if (confirmationDocument_) {
            confirmationDocument_->AddEventListener("click", this);
            confirmation_.Attach(confirmationDocument_);
            confirmationDocument_->Hide();
        }
    }
    playPresenter_.Attach(document_);
    document_->AddEventListener("click", this);
    document_->AddEventListener("change", this);
    document_->Hide();
    context_->Update();
    return true;
}

void NativeMenuUi::Shutdown() {
    confirmation_.Attach(nullptr);
    if (confirmationContext_) {
        if (confirmationDocument_) confirmationDocument_->RemoveEventListener("click",this);
        confirmationContext_->UnloadAllDocuments();
        Rml::RemoveContext("locus-confirmation");
        confirmationContext_ = nullptr;
        confirmationDocument_ = nullptr;
    }
    confirmationPressed_ = false;
    if (context_) {
        if (document_) {
            document_->RemoveEventListener("click", this);
            document_->RemoveEventListener("change", this);
        }
        if (homeDocument_) homeDocument_->RemoveEventListener("click", this);
        if (settingsDocument_)
            settingsDocument_->RemoveEventListener("click", this);
        settingsPresenter_.Attach(nullptr);
        settingsDocument_ = nullptr;
        if (editorDocument_) {
            editorDocument_->RemoveEventListener("click", this);
            editorDocument_->RemoveEventListener("change", this);
        }
        editorPresenter_.Attach(nullptr);
        editorDocument_ = nullptr;
        if (resultsDocument_) resultsDocument_->RemoveEventListener("click", this);
        resultsPresenter_.Attach(nullptr);
        resultsDocument_ = nullptr;
        if (skinDocument_) {
            skinDocument_->RemoveEventListener("click",this);
            skinDocument_->RemoveEventListener("focus",this,true);
            skinDocument_->RemoveEventListener("blur",this,true);
        }
        skinPresenter_.Attach(nullptr);
        skinDocument_ = nullptr;
        playPresenter_.Attach(nullptr);
        document_ = nullptr;
        homeDocument_ = nullptr;
        context_->UnloadAllDocuments();
        Rml::RemoveContext("locus-menu");
        context_ = nullptr;
        Rml::Shutdown();
        renderer_.ReleaseAllTextures();
    }
    visiblePage_ = Page::None;
    lastWidth_ = lastHeight_ = -1;
    lastDensity_ = -1.0f;
    pendingHomeAction_ = -1;
    editorPointerRouter_.Reset();

}

void NativeMenuUi::Activate(Page page) {
    if (visiblePage_ == page) return;
    editorPointerRouter_.Reset();
    // Never keep keyboard focus in a document that is about to be hidden.
    // Otherwise Enter/Space on the next screen may target stale controls.
    if (context_) {
        if (auto* focused = context_->GetFocusElement()) focused->Blur();
    }
    if (document_) document_->Hide();
    if (homeDocument_) homeDocument_->Hide();
    if (settingsDocument_) settingsDocument_->Hide();
    if (editorDocument_) editorDocument_->Hide();
    if (resultsDocument_) resultsDocument_->Hide();
    if (skinDocument_) skinDocument_->Hide();
    visiblePage_ = page;
    if (page == Page::Home && homeDocument_) {
        homeDocument_->Show();
        lastHomeNotice_.clear();
        if (auto* notice = homeDocument_->GetElementById("home-notice"))
            notice->SetInnerRML("");
    } else if (page == Page::Play && document_) {
        document_->Show();
        playPresenter_.Attach(document_);
    } else if (page == Page::Settings && settingsDocument_) {
        settingsDocument_->Show();
        settingsPresenter_.Attach(settingsDocument_);
    } else if (page == Page::Skin && skinDocument_) {
        skinDocument_->Show();
        skinPresenter_.Attach(skinDocument_);
    } else if (page == Page::Results && resultsDocument_) {
        resultsDocument_->Show();
        resultsPresenter_.Attach(resultsDocument_);
    } else if (page == Page::Editor && editorDocument_) {
        editorDocument_->Show();
        editorPresenter_.Attach(editorDocument_);
        lastEditorLayoutTab_ = -1;
    }
    ::SetMouseCursor(MOUSE_CURSOR_DEFAULT);
}

bool NativeMenuUi::ShowingHome() const noexcept {
    return visiblePage_ == Page::Home;
}

bool NativeMenuUi::ShowingPlay() const noexcept {
    return visiblePage_ == Page::Play;
}

bool NativeMenuUi::ShowingSettings() const noexcept {
    return visiblePage_ == Page::Settings;
}
bool NativeMenuUi::ShowingEditor() const noexcept {
    return visiblePage_ == Page::Editor;
}
bool NativeMenuUi::EditorTextFocused() const noexcept {
    const auto* focus = context_ ? context_->GetFocusElement() : nullptr;
    return ShowingEditor() && focus && focus->GetTagName() == "input";
}
bool NativeMenuUi::ShowingResults() const noexcept {
    return visiblePage_ == Page::Results;
}
bool NativeMenuUi::ShowingSkin() const noexcept {
    return visiblePage_ == Page::Skin;
}
Rectangle NativeMenuUi::SkinColorWheelBounds() const noexcept {
    if (!ShowingSkin() || !skinDocument_) return {};
    return RmlElementBounds(skinDocument_->GetElementById("skin-wheel-canvas"));
}
Rectangle NativeMenuUi::SkinColorWheelClipBounds() const noexcept {
    if (!ShowingSkin() || !skinDocument_) return {};
    if (auto* controls = skinDocument_->GetElementById("skin-controls")) {
        return RmlElementBounds(controls->GetParentNode());
    }
    return {};
}
Rectangle NativeMenuUi::SkinPreviewBounds() const noexcept {
    if (!ShowingSkin() || !skinDocument_) return {};
    return RmlElementBounds(skinDocument_->GetElementById("skin-preview-canvas"));
}

Rectangle NativeMenuUi::HomeWordmarkBounds() const noexcept {
    if (!ShowingHome() || !homeDocument_) return {};
    return RmlElementBounds(homeDocument_->GetElementById("home-wordmark"));
}

void NativeMenuUi::Hide() {
    if (visiblePage_ == Page::None) return;
    Activate(Page::None);
}

void NativeMenuUi::ProcessEvent(Rml::Event& event) {
    auto* target = event.GetTargetElement();
    if (!target) return;
    if (ShowingConfirmation()) {
        if (event.GetType()=="click") confirmation_.Click(target);
        return;
    }
    if (visiblePage_ == Page::Home) {
        if (event.GetType() != "click") return;
        while (target && !target->HasAttribute("data-action"))
            target = target->GetParentNode();
        if (!target) return;
        const std::string action =
            target->GetAttribute<Rml::String>("data-action", "");
        if (action == "home-play") pendingHomeAction_ = 0;
        else if (action == "home-editor") pendingHomeAction_ = 1;
        else if (action == "home-skins") pendingHomeAction_ = 2;
        else if (action == "home-settings") pendingHomeAction_ = 3;
        else if (action == "home-exit") pendingHomeAction_ = 4;
        return;
    }
    if (visiblePage_ == Page::Settings) {
        settingsPresenter_.ProcessEvent(event);
        return;
    }
    if (visiblePage_ == Page::Editor) {
        editorPresenter_.ProcessEvent(event);
        return;
    }
    if (visiblePage_ == Page::Results) {
        resultsPresenter_.ProcessEvent(event);
        return;
    }
    if (visiblePage_ == Page::Skin) {
        skinPresenter_.ProcessEvent(event);
        return;
    }
    if (visiblePage_ == Page::Play) playPresenter_.ProcessEvent(event);
}

bool NativeMenuUi::RefreshLayoutMetrics() {
    const int width = GetScreenWidth(), height = GetScreenHeight();
    bool layoutChanged = false;
    if (width != lastWidth_ || height != lastHeight_) {
        layoutChanged = true;
        context_->SetDimensions({std::max(1,width),std::max(1,height)});
        if (confirmationContext_)
            confirmationContext_->SetDimensions({std::max(1,width),std::max(1,height)});
        lastWidth_ = width; lastHeight_ = height;
    }
    // Both native contexts and custom canvases share a readable UI scale.
    // Window size contributes to density, never to fixed element coordinates.
    const float density = NativeUiDensity(width, height, GetWindowScaleDPI().x);
    if (std::abs(density - lastDensity_) > 0.01f) {
        context_->SetDensityIndependentPixelRatio(density);
        if (confirmationContext_) confirmationContext_->SetDensityIndependentPixelRatio(density);
        lastDensity_ = density;
        layoutChanged = true;
    }
    return layoutChanged;
}

void NativeMenuUi::PumpInput(bool dispatchKeyboard, bool forceLayout,
                             bool dispatchEnter) {
    // Resolve new dimensions and DPI before dispatching pointer events.
    if (RefreshLayoutMetrics() || forceLayout) context_->Update();
    DispatchInput(dispatchKeyboard, true, true, dispatchEnter);
}

void NativeMenuUi::DispatchInput(bool dispatchKeyboard, bool dispatchButtons,
                                 bool dispatchWheel, bool dispatchEnter) {
    const int mods = Modifiers();
    const auto mouse = GetMousePosition();
    context_->ProcessMouseMove(static_cast<int>(mouse.x),
                               static_cast<int>(mouse.y), mods);
    if (dispatchButtons && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        context_->ProcessMouseButtonDown(0, mods);
    if (dispatchButtons && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        context_->ProcessMouseButtonUp(0, mods);
    const float wheel = GetMouseWheelMove();
    if (dispatchWheel && wheel != 0) context_->ProcessMouseWheel(-wheel, mods);
    // Exclusive key capture, manual offset and calibration must never leak
    // their keyboard events into RmlUi buttons or navigation.
    if (!dispatchKeyboard) return;
    for (int key = KEY_A; key <= KEY_Z; ++key) {
        const auto name = ToRmlKey(key);
        if (IsKeyPressed(key)) context_->ProcessKeyDown(name, mods);
        if (IsKeyReleased(key)) context_->ProcessKeyUp(name, mods);
    }
    for (int key = KEY_ZERO; key <= KEY_NINE; ++key) {
        const auto name = ToRmlKey(key);
        if (IsKeyPressed(key)) context_->ProcessKeyDown(name, mods);
        if (IsKeyReleased(key)) context_->ProcessKeyUp(name, mods);
    }
    for (const int key : kUiKeys) {
        if (!dispatchEnter && (key == KEY_ENTER || key == KEY_KP_ENTER)) continue;
        const auto name = ToRmlKey(key);
        if (IsKeyPressed(key)) context_->ProcessKeyDown(name, mods);
        if (IsKeyReleased(key)) context_->ProcessKeyUp(name, mods);
    }
    int codepoint = 0;
    while ((codepoint = GetCharPressed()) != 0)
        context_->ProcessTextInput(static_cast<Rml::Character>(codepoint));
}

void NativeMenuUi::UpdateEditor(EditorUiState& ui, FrameInput& input,
                                const std::string& title, bool dirty,
                                double rate, bool playing,
                                long long songTimeMs,
                                const core::ChartEditor& draft,
                                const GameSettings& settings,
                                const EditorSetView& setView) {
    if (!EditorAvailable()) return;
    Activate(Page::Editor);
    // The document now fills the actual context. No per-widget resize
    // arithmetic or virtual-canvas offsets are written into RCSS.
    input.editor.nativeChrome = true;
    // Native rate entry has no dropdown. The retired fallback widget must
    // never participate in routing a native gesture.
    ui.rateDropdownOpen = false;
    editorPresenter_.BeginFrame(ui, input);
    editorPresenter_.Sync(ui, title, dirty, rate, playing, songTimeMs,
                          &draft, &settings, &setView);
    const bool tabChanged = lastEditorLayoutTab_ != static_cast<int>(ui.tab);
    if (RefreshLayoutMetrics() || tabChanged) context_->Update();
    lastEditorLayoutTab_ = static_cast<int>(ui.tab);
    ReadEditorLayout(ui);
    const auto pointer = ui.layout.coordinates.ToVirtual(GetMousePosition());
    const bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool held = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    const auto dispatch = editorPointerRouter_.Route(
        ui.layout, pointer, pressed, held, released, false, ui.comparisonPickerOpen);
    if (ui.tab == EditorTab::Set && ui.active == EditorField::Title) {
        if (auto* name = dynamic_cast<Rml::ElementFormControlInput*>(
                editorDocument_->GetElementById("editor-difficulty-title"))) {
            name->SetValue(ui.buffer);
            name->Focus();
            name->Select();
            ui.active = EditorField::None;
            ui.buffer.clear();
        }
    }
    // The active RmlUi input is the only consumer of native character
    // events. The legacy chart controller receives no text that frame.
    const bool nativeTyping = EditorTextFocused();
    if (nativeTyping) {
        // Text owns keyboard only. Pointer presses must still reach
        // the clicked control or the canvas on the same gesture.
        const auto pointer = input.editor;
        input.editor = EditorControls{};
        input.editor.nativeChrome = true;
        input.editor.mouse = pointer.mouse;
        input.editor.mouseClick = pointer.mouseClick;
        input.editor.mouseHeld = pointer.mouseHeld;
        input.editor.mouseReleased = pointer.mouseReleased;
        input.editor.mouseWheel = pointer.mouseWheel;
        SuppressGlobalMenuShortcuts(input);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (auto* focus = context_->GetFocusElement()) {
                const auto mouse = GetMousePosition();
                if (!CheckCollisionPointRec(mouse, RmlElementBounds(focus)))
                    focus->Blur();
            }
        }
    }
    if (pressed && !dispatch.nativeButtons && !nativeTyping)
        if (auto* focus = context_->GetFocusElement()) focus->Blur();
    // Route the entire physical gesture to one owner BEFORE Rml receives
    // events. In particular, a comparison-popup dismissal cannot produce
    // a native click on release, and a native click cannot author a note.
    DispatchInput(nativeTyping && EditorTextFocused() && !ui.comparisonPickerOpen,
                  dispatch.nativeButtons, dispatch.nativeWheel);
    // Enter commits any focused field; Esc reverts only that field.
    if (nativeTyping && EditorTextFocused()) {
        if (IsKeyPressed(KEY_ESCAPE)) editorPresenter_.CancelFocusedField();
        else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
            context_->GetFocusElement()->Blur();
    }
    if (ui.comparisonPickerOpen) {
        auto* panel = editorDocument_->GetElementById("editor-compare-panel");
        if (IsKeyPressed(KEY_TAB))
            MoveRmlModalFocus(panel,context_->GetFocusElement(),
                IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
            ActivateRmlModalButton(panel,context_->GetFocusElement());
    } else if (!ui.EditingText() && !nativeTyping) {
        if (IsKeyPressed(KEY_TAB)) {
            // Tab belongs to native focus traversal, not the old
            // editor's text-field navigation in the same frame.
            context_->ProcessKeyDown(Rml::Input::KI_TAB, Modifiers());
            input.editor.tab = false;
        }
        if (auto* focus = context_->GetFocusElement();
            focus && focus->GetTagName() == "button" &&
            (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
            if (ui.tab == EditorTab::Set &&
                focus->GetAttribute<Rml::String>("data-action", "") == "set-difficulty" &&
                focus->GetAttribute<Rml::String>("data-id", "") == ui.setSelectedId)
                input.editor.enterChart = true;
            else
                focus->Click();
            input.editor.enter = false;
        }
    }
    if (dispatch.nativeButtons) {
        input.editor.mouseClick = false;
        input.editor.mouseHeld = false;
        input.editor.mouseReleased = false;
        if (pressed || held || released) input.startPressed = false;
        if (pressed) input.backPressed = false;
    }
    if (dispatch.nativeWheel) input.editor.mouseWheel = 0.0f;
    input.editor.mouse = pointer;
    // The later SyncEditor call publishes the post-command render
    // geometry; this input frame uses the pre-dispatch snapshot.
    editorPresenter_.EndFrame();
}
void NativeMenuUi::ReadEditorLayout(EditorUiState& ui) const noexcept {
    EditorLayoutSnapshot snapshot;
    snapshot.native = true;
    snapshot.coordinates = ViewportTransform::NativeUi(lastDensity_);
    const auto slot = [&](const char* id) -> Rectangle {
        return snapshot.coordinates.ToVirtualBounds(
            RmlElementBounds(editorDocument_->GetElementById(id)));
    };
    snapshot.overview = slot("editor-overview-slot");
    snapshot.status = ui.tab == EditorTab::Set ? slot("editor-status-slot") : Rectangle{};
    snapshot.timeline = ui.tab == EditorTab::Route ? slot("editor-note-ruler") : Rectangle{};
    snapshot.preview = ui.tab == EditorTab::Route ? slot("editor-preview-slot") : Rectangle{};
    // Compare is a native button/modal, not a custom canvas slot.
    snapshot.compareToggle = {};
    ui.layout = snapshot;
}

void NativeMenuUi::SyncEditor(EditorUiState& ui,
                              const std::string& title, bool dirty,
                              double rate, bool playing,
                              long long songTimeMs,
                              const core::ChartEditor& draft,
                              const GameSettings& settings,
                              const EditorSetView& setView) {
    if (!ShowingEditor() || !EditorAvailable()) return;
    RefreshLayoutMetrics(); // also refresh during modal/transition-only frames
    editorPresenter_.Sync(ui, title, dirty, rate, playing, songTimeMs,
                          &draft, &settings, &setView);
    context_->Update();
    ReadEditorLayout(ui);
}

void NativeMenuUi::UpdateSettings(GameSettings& settings, SettingsUi& ui,
                                  FrameInput& input, MenuEvents& events,
                                  bool exclusiveKeyboardInput) {
    if (!SettingsAvailable()) return;
    Activate(Page::Settings);
    settingsPresenter_.BeginFrame(settings, ui, input, events);
    settingsPresenter_.Sync();
    const bool keyboard = !exclusiveKeyboardInput &&
        ui.capturing < 0 && !ui.editingGlobalOffset && !ui.calibrationOpen;
    PumpInput(keyboard);
    if (keyboard) {
        if (auto* focus = context_->GetFocusElement();
            focus && focus->GetTagName() == "button" &&
            (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
            focus->Click();
    }
    // A focused Settings action must not trigger global menu shortcuts.
    if (ui.capturing >= 0 || ui.editingGlobalOffset || ui.calibrationOpen) {
        SuppressGlobalMenuShortcuts(input);
    } else if (auto* focus = context_->GetFocusElement();
               focus && focus->GetTagName() == "button") {
        // Keep keyboard Back, but never activate another screen's shortcuts
        // while a control is focused.
        SuppressGlobalMenuShortcuts(input, true);
    }
    settingsPresenter_.Sync();
    context_->Update();
    settingsPresenter_.EndFrame();
}

void NativeMenuUi::UpdateHome(FrameInput& input, const std::string& status,
                              const std::string& profileLabel) {
    if (!HomeAvailable()) return;
    Activate(Page::Home);
    pendingHomeAction_ = -1;
    if (profileLabel!=lastProfileLabel_) {
        if (auto* element=homeDocument_->GetElementById("home-profile"))
            UpdateRmlText(element,profileLabel);
        lastProfileLabel_=profileLabel;
    }
    const bool failed = status.find("FAILED") != std::string::npos ||
        status.find("UNAVAILABLE") != std::string::npos ||
        status.find("NOT SAVED") != std::string::npos;
    const std::string notice = failed ? status : "";
    if (notice != lastHomeNotice_) {
        if (auto* element = homeDocument_->GetElementById("home-notice"))
            UpdateRmlText(element,notice);
        lastHomeNotice_ = notice;
    }
    PumpInput();
    // RmlUi activates focused buttons with Space; support Enter as well,
    // without allowing the Home default shortcut to steal focus activation.
    if (auto* focus = context_->GetFocusElement();
        focus && focus->GetTagName() == "button" &&
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
        focus->Click();
    if (auto* focused = context_->GetFocusElement();
        focused && focused->GetTagName() == "button")
        SuppressGlobalMenuShortcuts(input);
    switch (pendingHomeAction_) {
        case 0: input.requestedScreen = Screen::SongSelect; break;
        case 1: input.requestedScreen = Screen::Editor; break;
        case 2: input.requestedScreen = Screen::Skin; break;
        case 3: input.requestedScreen = Screen::Settings; break;
        case 4: input.closeRequested = true; break;
        default: break;
    }
    // Enter on a focused button cannot double-trigger the Home PLAY shortcut.
    const auto* focus = context_->GetFocusElement();
    if (pendingHomeAction_ >= 0 ||
        (focus && focus->GetTagName() == "button"))
        input.startPressed = false;
    context_->Update();
}

MenuEvents NativeMenuUi::Update(const SongLibrary& library, SongSelectUi& ui,
                                FrameInput& input) {
    if (!Available()) return {};
    if (visiblePage_ != Page::Play) {
        Activate(Page::Play);
        playPresenter_.SetSearch(ui.search);
    }
    const float oldDensity = lastDensity_;
    const auto scroll = playPresenter_.ReadScrollPosition(oldDensity);
    if (RefreshLayoutMetrics()) {
        context_->Update();
        if (oldDensity > 0.0f && oldDensity != lastDensity_)
            playPresenter_.RestoreScrollPosition(scroll,lastDensity_);
    }
    playPresenter_.BeginFrame(library,ui,lastDensity_);
    playPresenter_.Sync();
    // The library presenter owns Enter (including keypad Enter). RmlUi
    // must not also activate the focused row or action on the same press.
    // Apply rebuilt history row geometry before hit-testing mouse events.
    PumpInput(true, true, false);
    playPresenter_.ProcessKeyboard(input);
    playPresenter_.Sync();
    context_->Update();
    return playPresenter_.EndFrame();
}

MenuEvents NativeMenuUi::UpdateSkin(Skin& draft, SkinUi& ui,
                                      FrameInput& input) {
    MenuEvents events;
    if (!SkinAvailable()) return events;
    Activate(Page::Skin);
    const auto fileShortcut = SkinFileShortcut(input);
    const bool backShortcut = input.backPressed;
    SuppressGlobalMenuShortcuts(input);
    skinPresenter_.BeginFrame(draft, ui, input, events);
    skinPresenter_.Sync();
    const bool modalWasOpen = !ui.pendingDeleteProfile.empty();
    auto* modal = skinDocument_->GetElementById("skin-delete-panel");
    auto* cancel = skinDocument_->GetElementById("skin-delete-cancel");
    if (modalWasOpen && !RmlFocusWithin(modal,context_->GetFocusElement())) {
        context_->Update();
        FocusRmlModalControlIfOutside(modal,cancel);
    }
    PumpInput(!modalWasOpen);
    skinPresenter_.UpdatePointer(SkinColorWheelBounds());
    auto* focus = context_->GetFocusElement();
    const bool editing = focus && focus->GetTagName() == "input";
    if (modalWasOpen || !ui.pendingDeleteProfile.empty()) {
        // The confirmation is exclusive, including keyboard activation.
        if (modalWasOpen && !ui.pendingDeleteProfile.empty()) {
            if (IsKeyPressed(KEY_Y)) events.skinCommand = SkinCommand::DeleteProfile;
            else if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE)) {
                ui.pendingDeleteProfile.clear();
                ui.notice = "SKIN DELETION CANCELED";
                ui.noticeError = false;
            } else if (IsKeyPressed(KEY_TAB))
                MoveRmlModalFocus(modal,focus,
                    IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
            else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                ActivateRmlModalButton(modal,focus);
        }
    } else if (editing && IsKeyPressed(KEY_ESCAPE)) {
        skinPresenter_.FinishFocusedText(false);
    } else if (editing && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
        skinPresenter_.FinishFocusedText(true);
    } else if (!editing) {
        if (focus && focus->GetTagName() == "button" &&
            (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
            if (!focus->HasAttribute("disabled")) focus->Click();
        } else if (events.skinCommand == SkinCommand::None) {
            if (backShortcut) input.backPressed = true;
            else events.skinCommand = fileShortcut;
        }
    }
    skinPresenter_.Sync();
    context_->Update();
    if (!ui.pendingDeleteProfile.empty())
        FocusRmlModalControlIfOutside(modal,cancel);
    skinPresenter_.EndFrame();
    return events;
}

MenuEvents NativeMenuUi::UpdateResults(FrameInput& input,
    const RunResult* selected, const core::GradeCounts& grades, int total,
    const std::string& chartTitle, bool historical, const SongSelectUi& history) {
    MenuEvents events;
    if (!ResultsAvailable()) return events;
    Activate(Page::Results);
    const float oldDensity = lastDensity_;
    const float scrollDp = resultsPresenter_.ReadScrollDp(oldDensity);
    if (RefreshLayoutMetrics()) {
        context_->Update();
        if (oldDensity > 0.0f && oldDensity != lastDensity_)
            resultsPresenter_.RestoreScrollDp(scrollDp,lastDensity_);
    }
    const auto keyboard = TakeResultsKeyboard(input);
    resultsPresenter_.BeginFrame(selected, grades, total, chartTitle,
                                 historical, history, input, events);
    resultsPresenter_.Sync();
    PumpInput(true,true,false);
    bool consumed = input.startPressed || input.openEditorPressed ||
                    events.viewResultIndex >= 0 || events.watchReplay;
    if (!consumed && keyboard.activate) {
        if (auto* focus = context_->GetFocusElement();
            focus && focus->GetTagName() == "button") {
            consumed = true;
            if (!focus->HasAttribute("disabled")) focus->Click();
        }
    }
    ApplyResultsKeyboard(input,keyboard,historical,consumed);
    context_->Update();
    resultsPresenter_.Sync();
    context_->Update();
    resultsPresenter_.EndFrame();
    return events;
}

void NativeMenuUi::SyncConfirmation(const ConfirmationView& view) {
    if (!ConfirmationAvailable()) return;
    const bool wasVisible = confirmation_.Visible();
    const bool changed = !confirmation_.SamePrompt(view);
    if (changed) {
        confirmationPressed_ = false;
        editorPointerRouter_.Reset();
        if (wasVisible) {
            // Retire the old overlay's pointer capture before its replacement.
            confirmationContext_->ProcessMouseMove(-1,-1,Modifiers());
            confirmationContext_->ProcessMouseButtonUp(0,Modifiers());
            confirmation_.TakeChoice();
        }
        if (view.visible && !wasVisible) confirmationDocument_->Show();
    }
    const bool modelChanged = confirmation_.Sync(view);
    if (view.visible && !wasVisible) {
        // Cancel a press in the underlying context without losing its text
        // focus; Cancel on the dialog returns to the same editing control.
        // ProcessEvent already routes exclusively to the visible overlay.
        context_->ProcessMouseMove(-1,-1,Modifiers());
        context_->ProcessMouseButtonUp(0,Modifiers());
    }
    if (!view.visible) {
        if (wasVisible) confirmationDocument_->Hide();
        return;
    }
    const bool metricsChanged = RefreshLayoutMetrics();
    if (metricsChanged) context_->Update();
    if (modelChanged || metricsChanged || changed) confirmationContext_->Update();
    confirmation_.FocusCancelIfOutside();
}

ConfirmationChoice NativeMenuUi::UpdateConfirmation(const ConfirmationView& view) {
    SyncConfirmation(view);
    if (!ShowingConfirmation()) return ConfirmationChoice::None;
    const int mods = Modifiers();
    const auto mouse = GetMousePosition();
    confirmationContext_->ProcessMouseMove(static_cast<int>(mouse.x),static_cast<int>(mouse.y),mods);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        confirmationPressed_ = true;
        confirmationContext_->ProcessMouseButtonDown(0,mods);
    }
    // A release belonging to the button that opened the modal is not
    // confirmation input. Require a fresh press in this context.
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && confirmationPressed_) {
        confirmationPressed_ = false;
        confirmationContext_->ProcessMouseButtonUp(0,mods);
    }
    const float wheel = GetMouseWheelMove();
    if (wheel != 0) confirmationContext_->ProcessMouseWheel(-wheel,mods);
    auto* panel = confirmation_.Panel();
    auto* focus = confirmationContext_->GetFocusElement();
    if (IsKeyPressed(KEY_TAB))
        MoveRmlModalFocus(panel,focus,IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT));
    else if (IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_KP_ENTER))
        ActivateRmlModalButton(panel,focus);
    const auto keyboard = ChooseConfirmation(view,IsKeyPressed(KEY_S),IsKeyPressed(KEY_Y),
        IsKeyPressed(KEY_N)||IsKeyPressed(KEY_ESCAPE));
    const auto pointer = confirmation_.TakeChoice();
    confirmationContext_->Update();
    return keyboard != ConfirmationChoice::None ? keyboard : pointer;
}

void NativeMenuUi::RenderConfirmation() {
    if (!ShowingConfirmation()) return;
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    confirmationContext_->Render();
    renderer_.EnableScissorRegion(false);
    EndBlendMode();
}

void NativeMenuUi::Render() {
    if (!context_ || visiblePage_ == Page::None) return;
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    context_->Render();
    renderer_.EnableScissorRegion(false);
    EndBlendMode();
}

} // namespace locus::app
