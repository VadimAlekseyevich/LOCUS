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
    check(EaseSmoothstep(-1.0) == 0.0 &&
          EaseSmoothstep(0.0) == 0.0 &&
          EaseSmoothstep(1.0) == 1.0 &&
          EaseSmoothstep(3.0) == 1.0,
          "curve clamps outside interval");
    check(std::abs(EaseSmoothstep(0.25) - 0.15625) < 0.00001,
          "fade uses non-linear smoothstep");
    AudioVolumeFade gain;
    gain.Silent();
    gain.FadeIn(10.0, kAudioFadeOutSeconds);
    check(gain.Gain(10.0) == 0.0f &&
          gain.Gain(10.1) == 0.0f &&
          gain.Gain(10.28) > 0.99f,
          "incoming music stays silent during outgoing fade");
    gain.Shift(0.50);
    check(gain.Gain(10.40) == 0.0f && gain.Gain(10.78) > 0.99f,
          "pause preserves the audio fade timeline");
    gain.FadeOut(10.90);
    check(gain.Gain(10.91) < 1.0f &&
          gain.Gain(10.91) > 0.0f &&
          gain.Gain(11.03) == 0.0f && gain.Finished(11.03),
          "outgoing music fades continuously to silence");
    if (ok) std::cout << "AUDIO_FADE_PASS smooth=1 sequential=1\n";
    return ok ? 0 : 1;
}
