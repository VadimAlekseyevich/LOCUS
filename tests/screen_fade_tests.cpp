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
    ScreenFadeTransition fade;
    fade.Begin(Screen::Home, Screen::SongSelect, 5.0);
    const auto start = fade.Frame(5.0), middle = fade.Frame(5.08);
    check(start.visibleScreen == Screen::SongSelect && start.opacity == 1.0f &&
          middle.visibleScreen == Screen::SongSelect &&
          middle.opacity > 0.0f && middle.opacity < 1.0f &&
          fade.Frame(5.20).opacity == 0.0f && !fade.Active(5.20),
          "destination renders without legacy fallback flash");
    fade.Begin(Screen::SongSelect, Screen::Play, 6.0);
    check(fade.Frame(6.0).visibleScreen == Screen::Play &&
          fade.Frame(6.0).opacity == 1.0f &&
          fade.Frame(6.11).opacity == 0.0f,
          "Play uses its short reveal");
    fade.Begin(Screen::Play, Screen::Results, 7.0);
    check(fade.Frame(7.0).visibleScreen == Screen::Results &&
          fade.Frame(7.0).opacity == 0.0f,
          "outro handles Play-to-Results alone");
    fade.Begin(Screen::Results, Screen::Home, 8.0);
    fade.Begin(Screen::Home, Screen::Settings, 8.03);
    check(fade.Frame(8.03).visibleScreen == Screen::Settings &&
          fade.Frame(8.03).opacity == 1.0f &&
          fade.Frame(8.3).opacity == 0.0f,
          "rapid navigation never resurrects obsolete page");
    fade.Reset(Screen::Settings);
    check(fade.Frame(50.0).opacity == 0.0f &&
          fade.Frame(50.0).visibleScreen == Screen::Settings,
          "reset clears overlay");
    if (ok) std::cout << "SCREEN_FADE_PASS easing=1 navigation=1 play=1\n";
    return ok ? 0 : 1;
}
