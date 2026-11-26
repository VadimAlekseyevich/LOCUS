#include "app/hit_flash.h"

#include <cmath>
#include <iostream>
#include <limits>

using namespace locus::app;
using namespace locus::core;

namespace {
bool Check(bool ok, const char* label) {
    if (!ok) std::cerr << "HIT_FLASH_FAIL " << label << '\n';
    return ok;
}
} // namespace

int main() {
    bool ok = true;
    HitFlash flash;
    ok &= Check(flash.Sample(5.0).intensity == 0.0f,
                "no light before a successful judgement");

    flash.Trigger(Grade::Perfect, 10.0);
    const auto immediate = flash.Sample(10.0);
    const auto halfway = flash.Sample(10.0 + HitFlash::kDurationSeconds / 2.0);
    ok &= Check(immediate.intensity == 1.0f &&
                immediate.progress == 0.0f &&
                halfway.progress > 0.49f && halfway.progress < 0.51f &&
                halfway.intensity < 0.30f && halfway.intensity > 0.20f,
                "perfect flash starts bright and fades smoothly");
    ok &= Check(flash.Sample(10.0 + HitFlash::kDurationSeconds).intensity == 0.0f &&
                flash.Sample(10.0 + 1.0).intensity == 0.0f,
                "effect disappears after 180 milliseconds");
    ok &= Check(flash.Sample(9.9).intensity == 0.0f,
                "no backwards-time flash");

    flash.Trigger(Grade::Good, 20.0);
    const float good = flash.Sample(20.0).intensity;
    flash.Trigger(Grade::Bad, 21.0);
    const float bad = flash.Sample(21.0).intensity;
    ok &= Check(good > bad && good < 1.0f && bad > 0.0f,
                "good and bad successes get increasingly subtle feedback");

    flash.Trigger(Grade::Miss, 21.01);
    ok &= Check(flash.Sample(21.01).intensity > 0.0f,
                "miss does not launch or overwrite a successful flash");
    flash.Reset();
    flash.Trigger(Grade::Miss, 30.0);
    ok &= Check(flash.Sample(30.0).intensity == 0.0f,
                "wrong/missed notes never start a flash");

    flash.Trigger(Grade::Perfect, std::numeric_limits<double>::quiet_NaN());
    ok &= Check(flash.Sample(30.0).intensity == 0.0f,
                "non-finite clock readings do not start a burst");
    flash.Trigger(Grade::Perfect, 40.0);
    ok &= Check(flash.Sample(std::numeric_limits<double>::infinity()).intensity == 0.0f,
                "non-finite render clocks are safe");
    flash.Reset();
    ok &= Check(flash.Sample(40.05).intensity == 0.0f,
                "pause, retry and scene exit can clear effects immediately");

    flash.Trigger(Judgement{Grade::Perfect,0,-0.025},50.0);
    flash.Trigger(Judgement{Grade::Good,0,0.08},50.0);
    flash.Trigger(Judgement{Grade::Miss,0,0.0,true},50.0);
    ok &= Check(flash.Errors().Size()==2 &&
                flash.Errors().At(0).errorSeconds==-0.025 &&
                flash.Errors().At(1).errorSeconds==0.08,
                "independent diagonal timing marks survive one input batch");
    ok &= Check(HitErrorHistory::Opacity(flash.Errors().At(0),50.0)==1.0f &&
                HitErrorHistory::Opacity(flash.Errors().At(0),52.0)==0.0f,
                "timing marks expire in wall time");
    for (int i=0;i<100;++i)
        flash.Trigger(Judgement{Grade::Perfect,0,0.001*i},53.0);
    ok &= Check(flash.Errors().Size()==HitErrorHistory::kCapacity &&
                std::abs(flash.Errors().At(0).errorSeconds-0.036)<1e-9 &&
                std::abs(flash.Errors().At(63).errorSeconds-0.099)<1e-9,
                "timing history has fixed capacity and chronological draw order");
    flash.Reset();
    ok &= Check(flash.Errors().Size()==0,"reset clears timing marks");

    if (!ok) return 1;
    std::cout << "HIT_FLASH_PASS success-only attached pulse, fading and reset\n";
    return 0;
}
