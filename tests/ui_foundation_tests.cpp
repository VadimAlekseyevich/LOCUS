#include "app/ui_button.h"
#include "app/ui_focus.h"
#include "app/ui_motion.h"
#include "app/ui_theme.h"
#include "app/ui_widgets.h"
#include "app/home_menu.h"

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
    const auto& theme = ApprovedUiTheme();
    ok &= Check(theme.background.r == 32 && theme.background.g == 33 &&
                theme.softBlue.r == 187 && theme.softBlue.g == 210 &&
                theme.rose.r == 233 && theme.rose.g == 90 &&
                theme.buttonRadiusPx == 17.0f,
                "locked palette and geometry");
    ok &= Check(UiOpacity(theme.rose, 0.0f).a == 0 &&
                UiOpacity(theme.rose, 1.0f).a == 255 &&
                UiMix(theme.background, theme.foreground, 0.0f).r == 32 &&
                UiMix(theme.background, theme.foreground, 1.0f).r == 240,
                "color transforms clamp and match approved theme");
    ok &= Check(Near(UiRoundness({0, 0, 160, 50}, 17), 0.68f) &&
                UiRoundness({0, 0, 160, 0}, 17) == 0.0f,
                "pixel radii are independent of screen layout");

    const auto primary = ApprovedButtonStyle(UiButtonVariant::Rose);
    const auto editor = ApprovedButtonStyle(UiButtonVariant::Blue);
    ok &= Check(primary.fill.r == theme.rose.r &&
                primary.text.r == theme.onAccent.r &&
                editor.fill.b == theme.softBlue.b,
                "semantic button variants have consistent contrast");

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
