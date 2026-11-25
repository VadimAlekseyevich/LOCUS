#include "app/ui_button.h"
#include "app/ui_focus.h"
#include "app/ui_motion.h"
#include "app/ui_theme.h"
#include "app/ui_widgets.h"
#include "app/home_menu.h"
#include "app/viewport.h"
#include "app/gameplay_hud.h"
#include "app/results_keyboard_input.h"
#include "app/menu_controller.h"

#include <cmath>
#include <iostream>

using namespace locus::app;

namespace {
bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "UI_FOUNDATION_FAIL " << message << '\n';
    return condition;
}
bool Near(float lhs, float rhs, float tolerance = 0.015f) {
    return std::abs(lhs - rhs) <= tolerance;
}
}

int main() {
    bool ok = true;
    {
        FrameInput input;
        ok &= Check(SkinFileShortcut(input)==SkinCommand::None,
                    "SKINS has no implicit default file action");
        input.editorOpen = true;
        ok &= Check(SkinFileShortcut(input)==SkinCommand::Load,
                    "SKINS consumes configured Open shortcut");
        input.editorSave = true;
        ok &= Check(SkinFileShortcut(input)==SkinCommand::Save,
                    "SKINS consumes configured Save shortcut");
        input.editorSaveAs = true;
        ok &= Check(SkinFileShortcut(input)==SkinCommand::SaveAs,
                    "SKINS consumes configured Save As before Save");
        SuppressGlobalMenuShortcuts(input);
        ok &= Check(SkinFileShortcut(input)==SkinCommand::None,
                    "focused fields and modal suppression clear skin file shortcuts");
    }
    {
        FrameInput input;
        input.startPressed = true;
        input.openEditorPressed = true;
        input.restartPressed = true;
        input.closeRequested = true;
        const auto keyboard = TakeResultsKeyboard(input);
        ok &= Check(!input.startPressed && !input.openEditorPressed &&
                    !input.restartPressed && input.closeRequested,
                    "RESULTS separates collected commands without swallowing OS close");
        input.openEditorPressed = true; // Native focused EDIT or pointer action.
        ApplyResultsKeyboard(input,keyboard,false,true);
        ok &= Check(input.openEditorPressed && !input.startPressed &&
                    !input.restartPressed,
                    "RESULTS native command wins over other keyboard commands");
        input = {};
        ApplyResultsKeyboard(input,keyboard,false,false);
        ok &= Check(input.openEditorPressed && !input.startPressed &&
                    !input.restartPressed,
                    "RESULTS configured Edit shortcut has one deterministic owner");
        input = {};
        ApplyResultsKeyboard(input,{true,true,false,false},false,false);
        ok &= Check(input.restartPressed && !input.startPressed,
                    "RESULTS accepts configured Retry shortcut");
        input = {};
        ApplyResultsKeyboard(input,{true,true,true,false},true,false);
        ok &= Check(!input.startPressed && !input.restartPressed && !input.openEditorPressed,
                    "historical result never starts or edits a live chart");
        ApplyResultsKeyboard(input,{true,true,true,true},true,false);
        ok &= Check(input.backPressed && !input.startPressed &&
                    !input.restartPressed && !input.openEditorPressed,
                    "historical result preserves configured Back/Home");
    }
    // Grades must fit entirely outside the route clip, or be hidden.
    for (const auto size : {Vector2{1920,1080},Vector2{2560,1080},
                            Vector2{800,450},Vector2{1280,1024},
                            Vector2{540,540},Vector2{900,1200}}) {
        const auto transform = ViewportTransform::Gameplay(
            static_cast<int>(size.x),static_cast<int>(size.y));
        const auto bounds = transform.ToVirtualBounds({0,0,size.x,size.y});
        const Rectangle field{210,0,540,540};
        const auto l = LayoutGameplayHud(bounds,field);
        const auto fits = [&](Rectangle r) {
            return r.x >= bounds.x-0.01f && r.y >= bounds.y-0.01f &&
                r.x+r.width <= bounds.x+bounds.width+0.01f &&
                r.y+r.height <= bounds.y+bounds.height+0.01f;
        };
        const bool wide = size.x/size.y >= 1.5f;
        ok &= Check(fits(l.combo) && fits(l.accuracy) && fits(l.notice) &&
                    l.combo.y > bounds.y+bounds.height*0.5f &&
                    l.accuracy.y < bounds.y+bounds.height*0.5f &&
                    (wide ? l.grades.width > 0 && fits(l.grades) &&
                        l.grades.x >= field.x+field.width &&
                        l.grades.y >= l.accuracy.y+l.accuracy.height
                          : l.grades.width == 0),
                    "HUD corners remain visible; grades never overlap the world");
    }
    // The skin preview also passes its actual clipping rectangle.
    const auto previewHud = LayoutGameplayHud({605,193,298,248},{641,220,191,200});
    ok &= Check(previewHud.grades.width > 0 && previewHud.grades.x >= 832 &&
                previewHud.grades.x+previewHud.grades.width <= 903,
                "skin preview grades remain outside its actual route clip");
    // Regression: a 100%-DPI fullscreen window must not use 1px/dp.
    ok &= Check(Near(NativeUiDensity(960,540,1),1) &&
                Near(NativeUiDensity(1536,960,1),1.6f) &&
                Near(NativeUiDensity(1920,1200,1),2) &&
                Near(NativeUiDensity(1920,1080,0),2) &&
                Near(NativeUiDensity(2560,1080,1),2) &&
                Near(NativeUiDensity(3840,2160,1),3) &&
                Near(NativeUiDensity(800,450,2),1.125f) &&
                Near(NativeUiDensity(900,1200,2),900.0f/640.0f) &&
                Near(NativeUiDensity(1280,720,1.5f),1.5f) &&
                Near(NativeUiDensity(640,400,3),1),
                "shared UI scale grows on large 100%-DPI windows and respects usable area");
    const auto landscape = NativeUiDensity(1920,1080,1);
    const auto ultrawide = NativeUiDensity(2560,1080,1);
    ok &= Check(Near(landscape,ultrawide) &&
                2560/ultrawide > 1920/landscape &&
                3840/NativeUiDensity(3840,2160,1) > 1920/landscape,
                "larger or wider displays expose actual layout space, not letterboxing");
    const auto native = ViewportTransform::NativeUi(1.5f);
    const auto measured = native.ToVirtualBounds({300,150,900,450});
    const auto cursor = native.ToVirtual({750,375});
    const auto clip = native.Scissor(measured);
    ok &= Check(Near(native.offsetX,0) && Near(native.offsetY,0) &&
                Near(measured.x,200) && Near(measured.y,100) &&
                Near(measured.width,600) && Near(measured.height,300) &&
                Near(cursor.x,500) && Near(cursor.y,250) &&
                clip.x==300 && clip.y==150 && clip.width==900 && clip.height==450,
                "native measured bounds, pointer and render scissor use one origin and scale");
    const auto& theme = ApprovedUiTheme();
    ok &= Check(theme.background.r == 28 && theme.background.g == 33 &&
                theme.softBlue.r == 162 && theme.softBlue.g == 216 &&
                theme.rose.r == 181 && theme.rose.g == 164 &&
                theme.rose.b > theme.rose.r &&
                theme.softBlue.b > theme.softBlue.r &&
                theme.buttonRadiusPx == 14.0f,
                "nightbloom palette and control geometry");
    ok &= Check(UiOpacity(theme.rose, 0.0f).a == 0 &&
                UiOpacity(theme.rose, 1.0f).a == 255 &&
                UiMix(theme.background, theme.foreground, 0.0f).r ==
                    theme.background.r &&
                UiMix(theme.background, theme.foreground, 1.0f).r ==
                    theme.foreground.r,
                "color transforms clamp and match approved theme");
    ok &= Check(Near(UiRoundness({0, 0, 160, 50}, theme.buttonRadiusPx),
                     2.0f * theme.buttonRadiusPx / 50.0f) &&
                UiRoundness({0, 0, 160, 0}, theme.buttonRadiusPx) == 0.0f,
                "pixel radii are independent of screen layout");

    const auto primary = ApprovedButtonStyle(UiButtonVariant::Rose);
    const auto editor = ApprovedButtonStyle(UiButtonVariant::Blue);
    ok &= Check(primary.fill.r == theme.rose.r &&
                primary.text.r == theme.onAccent.r &&
                editor.fill.b == theme.softBlue.b,
                "semantic button variants have consistent contrast");

    const auto cool = ApprovedButtonStyle(UiButtonVariant::Cool);
    const auto warm = ApprovedButtonStyle(UiButtonVariant::Warm);
    const auto exitStyle = ApprovedButtonStyle(UiButtonVariant::Quiet);
    ok &= Check(cool.fill.b > theme.surface.b &&
                warm.fill.r > theme.surface.r &&
                warm.fill.b > theme.surface.b &&
                cool.selectedFill.b > cool.fill.b &&
                warm.selectedFill.b > warm.fill.b &&
                exitStyle.fill.a == 255 &&
                exitStyle.stroke.a > 0 &&
                exitStyle.text.r == theme.foreground.r,
                "Home secondary actions and Exit have visible chrome");

    UiMotionState animated;
    animated.Update({true, false, false, false, false}, 10.0);
    ok &= Check(Near(animated.Frame(10.0).hover, 0.0f) &&
                animated.Frame(10.065).hover > 0.45f &&
                animated.Frame(10.2).hover == 1.0f,
                "hover easing uses wall-clock time and reaches target");
    animated.Update({true, true, true, false, false}, 10.2);
    ok &= Check(animated.Frame(10.24).press > 0.45f &&
                animated.Frame(10.38).focus == 1.0f,
                "press and keyboard focus animate independently");
    animated.Update({false, false, false, false, true}, 10.4, true);
    ok &= Check(animated.Frame(10.4).hover == 0.0f &&
                animated.Frame(10.4).press == 0.0f &&
                animated.Frame(10.4).focus == 0.0f,
                "disabled or reduced-motion states are immediate");

    UiDialogMotion dialog;
    dialog.SetOpen(true, 20.0);
    ok &= Check(dialog.Opacity(20.0) == 0.0f &&
                dialog.Opacity(20.20) == 1.0f &&
                dialog.OffsetY(20.20) == 0.0f,
                "modal entrance is a presentation-only transition");
    dialog.SetOpen(false, 20.20);
    ok &= Check(Near(dialog.Opacity(20.34), 0.0f),
                "modal exit reaches invisible state");
    dialog.SetOpen(true, 21.0, true);
    ok &= Check(dialog.Opacity(21.0) == 1.0f,
                "reduced-motion modal is immediately visible");

    UiFocusChain focus;
    focus.Begin();
    ok &= Check(focus.Add(10, {0, 0, 100, 40}) &&
                focus.Add(20, {0, 50, 100, 40}, true) &&
                focus.Add(30, {0, 100, 100, 40}) &&
                !focus.Add(10, {0, 0, 100, 40}),
                "stable focus IDs reject duplicates");
    ok &= Check(focus.Move() && focus.Focused() == 10 &&
                focus.Move() && focus.Focused() == 30 &&
                focus.Move() && focus.Focused() == 10 &&
                focus.Move(true) && focus.Focused() == 30,
                "keyboard traversal wraps and skips disabled entries");
    ok &= Check(!focus.Focus(20) &&
                focus.Activate().has_value() &&
                *focus.Activate() == 30 &&
                UiPointerHit({0, 0, 100, 40}, {99, 39}) &&
                !UiPointerHit({0, 0, 100, 40}, {100, 39}),
                "keyboard activation and pointer hitboxes use fixed geometry");
    focus.Begin();
    focus.Add(10, {0, 0, 100, 40}, true);
    focus.Validate();
    ok &= Check(focus.Focused() == -1 && !focus.Activate().has_value(),
                "focus clears when its element becomes disabled");

    // Measurement is injected so Unicode clipping is verified headlessly.
    const auto monospaceWidth = [](const std::string& value) {
        return static_cast<float>(locus::core::utf8::Count(value) * 10);
    };
    ok &= Check(UiFitText(u8"абвгд", 40.0f, monospaceWidth) == u8"а..." &&
                UiFitText(u8"абвгд", 40.0f, monospaceWidth,
                          UiTextOverflow::EllipsisAtStart) == u8"...д" &&
                UiFitText("short", 60.0f, monospaceWidth) == "short" &&
                UiFitText("abcdef", 20.0f, monospaceWidth).empty(),
                "menu text fits actual glyph width and preserves UTF-8");
    // Home interactions use exactly the rectangles rendered at 960x540.
    HomeMenuUi home;
    HomeMenuInput homeInput{};
    homeInput.tabPressed = true;
    ok &= Check(home.Update(homeInput, 3.0) == -1 &&
                home.focus.Focused() == 0,
                "Home Tab starts with Play");
    homeInput.backwards = true;
    ok &= Check(home.Update(homeInput, 3.2) == -1 &&
                home.focus.Focused() == 4,
                "Home Shift+Tab reaches quiet Exit");
    homeInput.tabPressed = false;
    homeInput.backwards = false;
    homeInput.keyboardActivate = true;
    ok &= Check(home.Update(homeInput, 3.3) == 4,
                "Home Enter/Space activates focused Exit");
    homeInput.keyboardActivate = false;
    homeInput.pointerValid = true;
    homeInput.pointerPressed = true;
    homeInput.pointer = {HomeMenuButton(2).x + 8.0f,
                         HomeMenuButton(2).y + 9.0f};
    ok &= Check(home.Update(homeInput, 3.4) == 2 &&
                Near(home.motion[2].Frame(3.6).hover, 1.0f) &&
                HomeMenuButton(0).y == HomeMenuButton(3).y &&
                HomeMenuButton(4).y > HomeMenuButton(0).y,
                "Home click and menu-only hover share stable geometry");
    const float homeHoverAt30ms = home.motion[2].Frame(3.43).hover;
    ok &= Check(homeHoverAt30ms > 0.5f &&
                Near(home.motion[2].Frame(3.46).hover, 1.0f),
                "Home hover settles in 55ms, not the old 130ms");
    home.Reset();
    ok &= Check(!home.active && home.focus.Focused() == -1 &&
                Near(home.motion[2].Frame(4.0).hover, 0.0f),
                "Home focus/motion do not persist into other screens");

    UiTextBuffer input;
    input.maxCodepoints = 3;
    ok &= Check(input.Append("A") && input.Append("\xD0\xB1") &&
                input.Append("\xD1\x8F") &&
                !input.Append("z") && locus::core::utf8::Count(input.text) == 3,
                "text model respects Unicode code-point limit");
    input.Backspace();
    ok &= Check(locus::core::utf8::Count(input.text) == 2 &&
                locus::core::utf8::Valid(input.text),
                "backspace never splits UTF-8 scalar");
    ok &= Check(Near(UiSliderFraction(50, 0, 100), 0.5f) &&
                UiSliderFraction(0, 10, 10) == 0.0f &&
                Near(UiSliderFromPointer({10, 0, 200, 30},
                         {160, 12}, 0, 100, 10), 80.0f) &&
                UiSliderFromPointer({10, 0, 200, 30},
                    {999, 12}, 0, 100, 10) == 100,
                "slider mapping clamps and quantizes without moving hit area");

    if (ok) std::cout << "UI_FOUNDATION_PASS theme motion focus text sliders\n";
    return ok ? 0 : 1;
}
