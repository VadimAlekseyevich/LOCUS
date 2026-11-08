#include "app/screen_fade_transition.h"
#include <cmath>
#include <iostream>
using namespace locus::app;
int main() {
    bool ok = true;
    const auto check = [&](bool v, const char* what) {
        if (!v) std::cerr << "SCREEN_FADE_FAIL " << what << '\n';
        ok &= v;
    };
    check(EaseSmoothstep(-1.0) == 0.0 && EaseSmoothstep(1.0) == 1.0 &&
          std::abs(EaseMix(0.0f, 1.0f, 0.25) - 0.15625f) < 0.000001f,
          "shared nonlinear easing curve");
    check(EaseTime(10.05, 10.0, 0.1) > 0.4 &&
          EaseTime(10.05, 10.0, 0.1) < 0.6 &&
          EaseTime(10.05, 10.0, 0.0) == 0.0,
          "timed easing and zero duration");
    ScreenFadeTransition fade;
    fade.Begin(Screen::Home, Screen::SongSelect, 5.0);
    const auto a = fade.Frame(5.0), b = fade.Frame(5.035);
    const auto black = fade.Frame(5.065), c = fade.Frame(5.11);
    check(a.visibleScreen == Screen::Home && a.opacity == 0.0f &&
          b.visibleScreen == Screen::Home && b.opacity > 0.0f &&
          black.visibleScreen == Screen::SongSelect && black.opacity == 1.0f &&
          c.visibleScreen == Screen::SongSelect &&
          c.opacity > 0.0f && c.opacity < 1.0f &&
          fade.Frame(5.20).opacity == 0.0f && !fade.Active(5.20),
          "short two-phase transition swaps view when black");
    fade.Begin(Screen::SongSelect, Screen::Play, 6.0);
    check(fade.Frame(6.0).visibleScreen == Screen::Play &&
          fade.Frame(6.0).opacity == 1.0f &&
          fade.Frame(6.11).opacity == 0.0f,
          "Play is not delayed by old visual scene");
    fade.Begin(Screen::Play, Screen::Results, 7.0);
    check(fade.Frame(7.0).visibleScreen == Screen::Results &&
          fade.Frame(7.0).opacity == 0.0f,
          "outro handles Play-to-Results alone");
    fade.Begin(Screen::Results, Screen::Home, 8.0);
    fade.Begin(Screen::Results, Screen::Settings, 8.03);
    check(fade.Frame(8.03).visibleScreen == Screen::Results &&
          fade.Frame(8.3).visibleScreen == Screen::Settings,
          "rapid navigation uses currently visible screen");
    fade.Reset(Screen::Settings);
    check(fade.Frame(50.0).opacity == 0.0f &&
          fade.Frame(50.0).visibleScreen == Screen::Settings,
          "reset clears overlay");
    if (ok) std::cout << "SCREEN_FADE_PASS easing=1 navigation=1 play=1\n";
    return ok ? 0 : 1;
}
