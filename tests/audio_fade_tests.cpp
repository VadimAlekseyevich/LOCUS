#include "app/audio_volume_fade.h"

#include <cmath>
#include <iostream>

using namespace locus::app;
int main() {
    bool ok = true;
    const auto check = [&](bool pass, const char* label) {
        if (!pass) std::cerr << "AUDIO_FADE_FAIL " << label << '\n';
        ok &= pass;
    };
    check(SmoothAudioEase(-1.0) == 0.0 &&
          SmoothAudioEase(0.0) == 0.0 &&
          SmoothAudioEase(1.0) == 1.0 &&
          SmoothAudioEase(3.0) == 1.0,
          "curve clamps outside interval");
    check(std::abs(SmoothAudioEase(0.25) - 0.15625) < 0.00001,
          "fade uses non-linear smoothstep");
    AudioVolumeFade gain;
    gain.Silent();
    gain.FadeIn(10.0, kAudioFadeOutSeconds);
    check(gain.Gain(10.0) == 0.0f &&
          gain.Gain(10.1) == 0.0f &&
          gain.Gain(10.28) > 0.99f,
          "incoming music stays silent during outgoing fade");
    gain.FadeOut(10.40);
    check(gain.Gain(10.41) < 1.0f &&
          gain.Gain(10.41) > 0.0f &&
          gain.Gain(10.53) == 0.0f && gain.Finished(10.53),
          "outgoing music fades continuously to silence");
    if (ok) std::cout << "AUDIO_FADE_PASS smooth=1 sequential=1\n";
    return ok ? 0 : 1;
}
