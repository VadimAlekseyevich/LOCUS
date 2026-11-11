#pragma once

#include "app/game_settings.h"
#include "app/hit_flash.h"
#include "app/route_renderer.h"
#include "app/play_outro_transition.h"
#include "app/skin.h"
#include "app/viewport.h"
#include "core/judgement.h"
#include "core/route.h"
#include "core/route_view.h"
#include "raylib.h"

#include <algorithm>
#include <string>

namespace locus::app {

// One presentation snapshot for both generated and local-chart sessions.
struct GameplayView {
    const locus::core::Chart& chart;
    const locus::core::TurnJudge& judge;
    const locus::app::HitFlash& hitFlash;
    const std::string& title;
    double songSeconds;
    double offsetSeconds; // Song beat-zero offset, not a gameplay grid index.
    bool paused;
    bool completed;
    bool custom;
    double practiceSpeed;
    const char* status;
    bool showHitFlash = true;
    const locus::app::GameSettings& bindings;
    const locus::app::Skin& skin;
    double adjustmentNoticeUntil = -1.0;
    const char* adjustmentNotice = "";
    PlayOutroFrame outro{};
    // Computed from live player movement outside the renderer (never from
    // future chart geometry). False keeps original center-camera fallback.
    bool hasCameraAnchor = false;
    core::GridPoint cameraAnchor{};
    const core::TimedRoute* timedRoute = nullptr;
    // Frame-only borrow from PlayPresentation. The next Prepare may overwrite
    // this object; RenderFrame must not retain the pointer.
    const RoutePreviewFrame* routePreview = nullptr;
};

// The visible playfield is exactly one screen-height square, centred in
// the virtual 960x540 viewport: x=210..750 and y=0..540. The entire
// infinite route continues mathematically outside this clipped window.
inline constexpr Rectangle kGameplayWorldViewport{
    (kVirtualWidth - kVirtualHeight) * 0.5f, 0.0f,
    static_cast<float>(kVirtualHeight), static_cast<float>(kVirtualHeight)
};
// In landscape windows, the square uses the FULL physical window height,
// rather than the 16:9 menu letterbox. Portrait is necessarily width-limited.
inline float GameplayViewportScale(int width, int height) noexcept {
    return locus::app::ViewportTransform::Gameplay(width, height).scale;
}

// Actual visible sidebar area in virtual coordinates. PLAY scales to the
// window height, so fixed 960x540 HUD coordinates can otherwise be cropped
// in resizable windows narrower than 16:9.
inline float GameplayRightSidebarWidth(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return 0.0f;
    const float visibleWidth = width / GameplayViewportScale(width, height);
    return std::max(0.0f, (visibleWidth - kGameplayWorldViewport.width) * 0.5f);
}

struct GameplayCamera {
    static constexpr float kCenterX = kVirtualWidth * 0.5f;
    static constexpr float kCenterY = kVirtualHeight * 0.5f;
    // Camera translation is presentation-only; judgement uses song time.
    locus::core::GridPoint world{};
    float step = 1.0f;

    GameplayCamera(const locus::core::Chart& chart, double beat,
                   const locus::app::Skin& skin = locus::app::Skin{},
                   double practiceRate = 1.0)
        : step(StepForFixedSpeed(skin.baseSpeedPxPerSecond, chart.bpm,
                                 chart.ticksPerBeat, practiceRate)) {
        world = core::RoutePositionAt(chart, beat);
    }

    GameplayCamera(const core::TimedRoute& route, double songSeconds,
                   const locus::core::Chart& chart,
                   const locus::app::Skin& skin,
                   double practiceRate)
        : world(route.PositionAt(songSeconds)),
          step(StepForFixedSpeed(skin.baseSpeedPxPerSecond, chart.bpm,
                                 chart.ticksPerBeat, practiceRate)) {}

    GameplayCamera(core::GridPoint marker, float pixelsPerUnit)
        : world(marker), step(pixelsPerUnit) {}

    locus::app::RouteCanvas Canvas() const noexcept {
        return {kCenterX, kCenterY, step, world};
    }
};


void DrawPlayGrid(Font font, const GameplayView& gameplay, double now);
void DrawResultsScreen(Font font, const GameplayView& gameplay);

} // namespace locus::app
