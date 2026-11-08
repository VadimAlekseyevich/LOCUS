#include "app/app_smoke_checks.h"
#include "app/app_runtime_policy.h"
#include "app/chart_editor_layout.h"
#include "app/gameplay_ui.h"
#include "app/menu_ui.h"
#include "app/game_settings.h"
#include "core/route.h"
#include "core/route_view.h"
#include "core/judgement.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <utility>

namespace locus::app {
namespace {
constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
}
// Keep rendering-only layout regressions in the headless screen check.
// This uses the same rectangles as the visible controls and hit testing.
bool CheckPresentationLayout() {
    // Every edge of the gameplay window must form a fully visible,
    // centered 540x540 square; the marker sits at its exact midpoint.
    if (GameplayCamera::kCenterX != kWindowWidth * 0.5f ||
        GameplayCamera::kCenterY != kWindowHeight * 0.5f ||
        kGameplayWorldViewport.x != 210.0f ||
        kGameplayWorldViewport.y != 0.0f ||
        kGameplayWorldViewport.width != static_cast<float>(kWindowHeight) ||
        kGameplayWorldViewport.height != static_cast<float>(kWindowHeight) ||
        kGameplayWorldViewport.x + kGameplayWorldViewport.width != 750.0f ||
        kGameplayWorldViewport.x + kGameplayWorldViewport.width * 0.5f !=
            GameplayCamera::kCenterX ||
        kGameplayWorldViewport.y + kGameplayWorldViewport.height * 0.5f !=
            GameplayCamera::kCenterY)
        return false;
    for (const auto& size : {
             std::pair<int, int>{960, 540}, {1920, 1080},
             {1280, 800}, {800, 600}, {600, 800}}) {
        // In landscape the square's side equals the height of the actual
        // window; portrait windows cannot contain a height-wide square.
        const float scale = GameplayViewportScale(size.first, size.second);
        const float side = kGameplayWorldViewport.width * scale;
        const float expected = static_cast<float>(
            std::min(size.first, size.second));
        if (std::abs(side - expected) > 0.001f)
            return false;
        // The right-side statistics must never extend into invisible
        // letterbox/cropped space, even in a 4:3 window.
        const float rightWidth = GameplayRightSidebarWidth(
            size.first, size.second);
        const float projectedRight =
            (kGameplayWorldViewport.x +
             kGameplayWorldViewport.width + rightWidth -
             GameplayCamera::kCenterX) * scale +
            size.first * 0.5f;
        if (std::abs(projectedRight - size.first) > 0.01f ||
            rightWidth < 0.0f)
            return false;
    }
    const auto back = locus::app::detail::kEditorBack;
    const auto tab = locus::app::detail::kTabField;
    const auto exit = locus::app::HomeMenuButton(4);
    // Dedicated Back and Exit actions must stay visible and not overlap
    // the editor FIELD tab or the Home SETTINGS button.
    if (back.x < tab.x + tab.width ||
        back.x + back.width > kWindowWidth ||
        back.y + back.height > kWindowHeight ||
        exit.y < locus::app::HomeMenuButton(3).y +
                     locus::app::HomeMenuButton(3).height ||
        exit.y + exit.height > kWindowHeight)
        return false;
    const Rectangle left{52, 149, 408, 325};
    const Rectangle right{486, 149, 432, 325};
    const auto inside = [](Rectangle frame, Rectangle r) {
        return r.x >= frame.x && r.y >= frame.y &&
               r.x + r.width <= frame.x + frame.width &&
               r.y + r.height <= frame.y + frame.height;
    };
    const Rectangle leftControls[] = {
        locus::app::SettingsGameTab(), locus::app::SettingsDisplayTab(),
        locus::app::SettingsVolumeMinus(), locus::app::SettingsVolumePlus(),
        locus::app::SettingsFlashButton(),
        locus::app::SettingsChordMinus(), locus::app::SettingsChordPlus(),
        locus::app::SettingsVsyncButton(),
        locus::app::SettingsFpsMinus(), locus::app::SettingsFpsPlus(),
        locus::app::SettingsShowFpsButton(),
        locus::app::SettingsFullscreenButton()
    };
    for (const auto control : leftControls)
        if (!inside(left, control)) return false;
    for (int i = 0; i < locus::app::kGameActionCount; ++i) {
        const auto button = locus::app::SettingsBindingButton(i);
        if (!inside(right, button)) return false;
        if (button.y + button.height >= 474.0f) return false;
        if (i > 0) {
            const auto previous = locus::app::SettingsBindingButton(i - 1);
            if (button.y < previous.y + previous.height) return false;
        }
    }
    // Player remains centered and speed must not depend on chart grid size.
    for (int dimensions : {2, 4, 8, 16}) {
        auto chart = locus::core::MakeDemoChart();
        chart.gridWidth = dimensions;
        chart.gridHeight = dimensions;
        for (double beat : {0.0, 1.5, 4.75, 15.125, 30.5}) {
            const GameplayCamera camera(chart, beat);
            const Vector2 projected = camera.Canvas().Position(camera.world);
            const float expected = locus::app::StepForFixedSpeed(
                locus::app::kDefaultBaseSpeed, chart.bpm, chart.ticksPerBeat);
            if (std::abs(projected.x - GameplayCamera::kCenterX) > 0.001f ||
                std::abs(projected.y - GameplayCamera::kCenterY) > 0.001f ||
                std::abs(camera.step - expected) > 0.001f)
                return false;
        }
    }
    auto longRoute = locus::core::MakeDemoChart();
    longRoute.lengthBeats = 65536;
    longRoute.turns.clear();
    longRoute.initialHeading = locus::core::Heading::UpRight;
    const GameplayCamera farCamera(longRoute, 65321.625);
    const auto farSample = farCamera.Canvas().Position(farCamera.world);
    if (std::abs(farSample.x - GameplayCamera::kCenterX) > 0.001f ||
        std::abs(farSample.y - GameplayCamera::kCenterY) > 0.001f)
        return false;
    return true;
}

// Headless checks exercise the same update function without starting raylib.
bool CheckLoopState() {
    AppState state;
    for (int i = 0; i < kSmokeFrames; ++i) {
        UpdateState(state, FrameInput{}, true);
        if (!state.running || state.frames != i + 1 || state.screen != Screen::Home) {
            return false;
        }
    }
    UpdateState(state, FrameInput{}, true);
    if (state.running || state.frames != kSmokeFrames) {
        return false;
    }

    AppState closedByUser;
    UpdateState(closedByUser, FrameInput{true, std::nullopt}, false);
    if (!closedByUser.running || !closedByUser.quitConfirmation ||
        closedByUser.frames != 1) return false;
    FrameInput cancel;
    cancel.quitCanceled = true;
    UpdateState(closedByUser, cancel, false);
    if (!closedByUser.running || closedByUser.quitConfirmation ||
        closedByUser.frames != 2) return false;
    UpdateState(closedByUser, FrameInput{true, std::nullopt}, false);
    FrameInput approve;
    approve.quitConfirmed = true;
    UpdateState(closedByUser, approve, false);
    return !closedByUser.running && closedByUser.frames == 3;
}

bool CheckScreenTransitions() {
    AppState state;
    if (state.screen != Screen::Home || std::strcmp(ScreenName(state.screen), "Home") != 0) {
        return false;
    }

    const Screen transitions[] = {Screen::SongSelect, Screen::Play,
                                  Screen::Editor, Screen::Skin,
                                  Screen::Settings, Screen::Results, Screen::Home};
    int expectedFrames = 0;
    for (Screen next : transitions) {
        UpdateState(state, FrameInput{false, next}, false);
        if (!state.running || state.screen != next || ++expectedFrames != state.frames) {
            return false;
        }
    }

    // No key press means no screen transition.
    UpdateState(state, FrameInput{}, false);
    if (state.screen != Screen::Home || state.frames != ++expectedFrames) {
        return false;
    }

    // An X/Alt+F4 or Home Esc requires approval, taking priority over
    // any simultaneous navigation. No wrong screen should open underneath.
    UpdateState(state, FrameInput{true, Screen::Editor}, false);
    if (!state.running || !state.quitConfirmation ||
        state.screen != Screen::Home || state.frames != ++expectedFrames)
        return false;
    UpdateState(state, FrameInput{false, Screen::Play}, false);
    if (state.screen != Screen::Home || !state.quitConfirmation ||
        state.frames != ++expectedFrames) return false;
    FrameInput cancel;
    cancel.quitCanceled = true;
    UpdateState(state, cancel, false);
    if (!state.running || state.quitConfirmation ||
        state.screen != Screen::Home || state.frames != ++expectedFrames)
        return false;
    // Back from Editor is an ordinary screen navigation, not a close.
    state.screen = Screen::Editor;
    FrameInput back;
    back.backPressed = true;
    UpdateState(state, back, false);
    if (!state.running || state.screen != Screen::Home ||
        state.quitConfirmation || state.frames != ++expectedFrames)
        return false;
    // The confirmation can reopen repeatedly; only Y terminates.
    UpdateState(state, FrameInput{true, std::nullopt}, false);
    if (!state.running || !state.quitConfirmation ||
        state.frames != ++expectedFrames) return false;
    FrameInput approve;
    approve.quitConfirmed = true;
    UpdateState(state, approve, false);
    return CheckPresentationLayout() &&
           !state.running && state.screen == Screen::Home &&
           state.frames == expectedFrames &&
           std::strcmp(ScreenName(Screen::Play), "Play") == 0 &&
           std::strcmp(ScreenName(Screen::Editor), "Editor") == 0 &&
           std::strcmp(ScreenName(Screen::Results), "Results") == 0;
}
// Headless session check: start, return Home, retry, resolve the demo,
// and retry from Results. Desktop and real-audio behavior need manual testing.
bool CheckSessionFlow() {
    AppState state;
    const locus::core::Chart chart = locus::core::MakeDemoChart();
    locus::core::TurnJudge judge(chart);

    FrameInput start;
    start.startPressed = true;
    UpdateState(state, start, false);
    if (state.screen != Screen::SongSelect) return false;
    FrameInput chooseDemo;
    chooseDemo.requestedScreen = Screen::Play;
    UpdateState(state, chooseDemo, false);
    if (state.screen != Screen::Play) return false;

    // A turn can be judged without ever changing authored movement.
    const auto baseline = locus::core::SampleRouteStep(chart, 8);
    judge.Press(locus::core::Turn::Right, 1.0);
    if (judge.Counts().perfect != 1) return false;

    FrameInput goHome;
    goHome.homePressed = true;
    UpdateState(state, goHome, false);
    if (state.screen != Screen::Home) return false;
    UpdateState(state, start, false);
    if (state.screen != Screen::SongSelect) return false;
    UpdateState(state, chooseDemo, false);
    if (state.screen != Screen::Play) return false;
    judge.Reset();
    if (judge.Counts().Resolved() != 0) return false;

    // Audio finishing is no longer the primary completion condition.
    // Even a very long song finishes at the instant the final note is
    // judged (both correct hits and late-window misses).
    if (ChartReadyForResults(judge, false)) return false;
    judge.Advance(14.819); // The last demo note is at 15 seconds.
    if (ChartReadyForResults(judge, false)) return false;
    // Beat 30 is the last turn (LEFT) at 15.0 seconds. A hit here
    // ends immediately; the synthesized demo click track lasts 16 s.
    const auto finalHit = judge.Press(locus::core::Turn::Left, 15.0);
    if (!finalHit || finalHit->grade != locus::core::Grade::Perfect ||
        !ChartReadyForResults(judge, false)) return false;
    FinishSession(state, judge);
    if (state.screen != Screen::Results ||
        judge.Counts().perfect != 1 ||
        judge.Counts().miss != static_cast<int>(chart.turns.size()) - 1 ||
        judge.Counts().Resolved() != static_cast<int>(chart.turns.size())) {
        return false;
    }
    const auto unchanged = locus::core::SampleRouteStep(chart, 8);
    if (baseline.from.x != unchanged.from.x ||
        baseline.from.y != unchanged.from.y ||
        baseline.to.x != unchanged.to.x ||
        baseline.to.y != unchanged.to.y ||
        baseline.heading != unchanged.heading) return false;

    FrameInput retry;
    retry.restartPressed = true;
    UpdateState(state, retry, false);
    if (state.screen != Screen::Play) return false;
    judge.Reset();
    if (judge.Counts().Resolved() != 0) return false;

    // Even when audio has just ended, a pending last note must keep its
    // full late-hit window; completion requires a real final judgement.
    judge.Advance(15.17);
    if (ChartReadyForResults(judge, true)) return false;
    judge.Advance(15.181);
    if (!ChartReadyForResults(judge, true) ||
        judge.Counts().miss != static_cast<int>(chart.turns.size()) ||
        judge.Counts().AccuracyPercent() != 0.0)
        return false;

    // An empty chart (or a TEST beginning after its final note) has no
    // scorable notes, so only there may the song end trigger Results.
    locus::core::Chart emptyChart;
    locus::core::TurnJudge noNotes(emptyChart);
    if (ChartReadyForResults(noNotes, false) ||
        !ChartReadyForResults(noNotes, true))
        return false;
    locus::core::TurnJudge noTestNotes(chart);
    noTestNotes.ResetFromBeat(31);
    if (noTestNotes.ScorableCount() != 0 ||
        ChartReadyForResults(noTestNotes, false) ||
        !ChartReadyForResults(noTestNotes, true))
        return false;

    // Closing the window blocks other navigation, and only a confirmed
    // exit may terminate the current game session.
    FrameInput close;
    close.closeRequested = true;
    close.requestedScreen = Screen::Home;
    UpdateState(state, close, false);
    if (!state.running || !state.quitConfirmation ||
        state.screen != Screen::Play) return false;
    FrameInput canceled;
    canceled.quitCanceled = true;
    UpdateState(state, canceled, false);
    if (!state.running || state.quitConfirmation ||
        state.screen != Screen::Play) return false;
    UpdateState(state, close, false);
    FrameInput confirmed;
    confirmed.quitConfirmed = true;
    UpdateState(state, confirmed, false);
    return !state.running && state.screen == Screen::Play;
}

} // namespace locus::app
