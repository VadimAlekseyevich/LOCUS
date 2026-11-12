#pragma once

#include "app/ui_focus.h"
#include "app/ui_motion.h"

#include <array>

namespace locus::app {

// Approved Home reference: four centered actions and a quiet Exit.
// Every rectangle is in the shared virtual 960x540 UI canvas.
constexpr int kHomeMenuActionCount = 5;
inline Rectangle HomeMenuButton(int index) noexcept {
    if (index == 4) return {784.0f, 458.0f, 122.0f, 46.0f};
    if (index < 0 || index >= 4) return {};
    return {77.0f + 204.0f * index, 324.0f, 192.0f, 80.0f};
}

// Input is sampled by the menu controller, never from the renderer. Nothing
// in this component can poll or intercept gameplay inputs.
struct HomeMenuInput {
    Vector2 pointer{};
    bool pointerValid = false;
    bool pointerDown = false;
    bool pointerPressed = false;
    bool tabPressed = false;
    bool backwards = false;
    bool keyboardActivate = false;
    bool reducedMotion = false;
};

struct HomeMenuUi {
    UiFocusChain focus;
    std::array<UiMotionState, kHomeMenuActionCount> motion{};
    bool active = false;

    void Reset() noexcept {
        focus.Clear();
        for (auto& item : motion) item.Reset();
        active = false;
    }

    // Returns the clicked/activated action index, -1 if none.
    int Update(const HomeMenuInput& input, double now) {
        active = true;
        focus.Begin();
        for (int i = 0; i < kHomeMenuActionCount; ++i)
            focus.Add(i, HomeMenuButton(i));
        focus.Validate();
        if (input.tabPressed) focus.Move(input.backwards);

        int action = -1;
        if (input.pointerPressed && input.pointerValid) {
            for (int i = 0; i < kHomeMenuActionCount; ++i) {
                if (UiPointerHit(HomeMenuButton(i), input.pointer)) {
                    action = i;
                    break;
                }
            }
        }
        if (action == -1 && input.keyboardActivate) {
            const auto id = focus.Activate();
            if (id) action = *id;
        }

        // The previous 130 ms hover felt like a delayed response. Home
        // responds almost immediately; other menus keep their own timings.
        UiMotionDurations homeMotion{};
        homeMotion.hoverSeconds = 0.055;
        homeMotion.pressSeconds = 0.045;
        homeMotion.selectionSeconds = 0.095;
        for (int i = 0; i < kHomeMenuActionCount; ++i) {
            const bool hovered = input.pointerValid &&
                UiPointerHit(HomeMenuButton(i), input.pointer);
            UiInteraction state{};
            state.hovered = hovered;
            state.pressed = hovered && input.pointerDown;
            state.focused = focus.Focused() == i;
            motion[static_cast<std::size_t>(i)].Update(
                state, now, input.reducedMotion, homeMotion);
        }
        return action;
    }
};

} // namespace locus::app
