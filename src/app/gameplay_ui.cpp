#include "app/gameplay_ui.h"
#include "app/result_rank_color.h"
#include "app/gameplay_hud.h"
#include "app/path_mesh.h"
#include "core/utf8.h"

#include "app/visual_style.h"
#include "app/screen_action_buttons.h"
#include "app/ui_button.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace locus::app {
namespace {
void DrawTextLabel(Font font, const char* message, Vector2 position,
                   float size, Color color) {
    DrawTextEx(font, message, position, size, 1.0f, color);
}
} // namespace

void DrawPlayGrid(Font font,const GameplayView& gameplay,double now,
                  PathRibbonWorkspace* ribbon) {
    // Both route and marker use the SAME camera transform. The follower
    // only uses the player's current position and is updated by
    // PlayPresentation, not during rendering. Freeze during the outro.
    const double displaySeconds = gameplay.outro.active
        ? gameplay.outro.frozenSongSeconds : gameplay.songSeconds;
    // A normal PLAY frame borrows the immutable compiled route. The fallback
    // is for separately rendered transition frames that have no controller.
    core::TimedRoute transientRoute;
    const core::TimedRoute* route = gameplay.timedRoute;
    if (!route) {
        if (transientRoute.Compile(gameplay.chart, gameplay.offsetSeconds))
            route = &transientRoute;
    }
    if (!route) return;
    RoutePreviewFrame standalone;
    if (!gameplay.routePreview)
        PrepareRoutePreview(*route, displaySeconds, gameplay.skin.lookaheadMs,
                            standalone);
    const RoutePreviewFrame& preview = gameplay.routePreview
        ? *gameplay.routePreview : standalone;
    const float step = StepForTimedRoute(
        gameplay.skin.baseSpeedPxPerSecond, *route, gameplay.practiceSpeed);
    GameplayCamera camera(preview.marker, step);
    if (gameplay.hasCameraAnchor) camera.world = gameplay.cameraAnchor;
    const auto canvas = camera.Canvas();

    // Scissor coordinates are physical pixels while all world points remain
    // projected into the virtual 960x540 resolution.
    // Gameplay uses the actual window height, not the 16:9 letterboxed
    // UI fitting scale. For any landscape window, the square reaches both
    // top and bottom pixel edges; portrait windows are width-limited.
    const auto transform = locus::app::ViewportTransform::Gameplay(
        GetScreenWidth(), GetScreenHeight());
    const auto clip = transform.Scissor(kGameplayWorldViewport);
    BeginScissorMode(clip.x, clip.y, clip.width, clip.height);

    // PLAY and editor TEST: the camera gently trails player motion,
    // including turns. No predictive aiming, past trail, grid or debug UI.
    PathRibbonWorkspace standaloneRibbon;
    Vector2 marker=DrawTimedRouteTrajectory(preview,canvas,gameplay.skin,
        ribbon?*ribbon:standaloneRibbon);
    locus::app::DrawTimedRouteJunctions(
        preview, canvas, gameplay.skin);
    if (gameplay.outro.active) {
        Vector2 direction{};
        constexpr float d = 0.70710678f;
        using Heading = locus::core::Heading;
        switch (gameplay.outro.outgoing) {
            case Heading::Right: direction = {1.0f, 0.0f}; break;
            case Heading::Left: direction = {-1.0f, 0.0f}; break;
            case Heading::Up: direction = {0.0f, -1.0f}; break;
            case Heading::Down: direction = {0.0f, 1.0f}; break;
            case Heading::UpRight: direction = {d, -d}; break;
            case Heading::DownRight: direction = {d, d}; break;
            case Heading::DownLeft: direction = {-d, d}; break;
            case Heading::UpLeft: direction = {-d, -d}; break;
        }
        DrawSoftRouteLine(marker,
                   {marker.x + direction.x * 900.0f,
                    marker.y + direction.y * 900.0f},
                   std::max(1.0f, gameplay.skin.pathWidth * 0.7f),
                   locus::app::WithOpacity(gameplay.skin.trajectory, 0.65f));
        marker.x += direction.x * gameplay.outro.distancePx;
        marker.y += direction.y * gameplay.outro.distancePx;
    }
    const float markerRadius = gameplay.skin.playerRadius;
    if (gameplay.skin.playerGlow)
        DrawCircleV(marker, markerRadius * 1.65f,
                    locus::app::WithOpacity(gameplay.skin.player, 0.22f));
    DrawCircleV(marker, markerRadius, gameplay.skin.player);
    DrawCircleV(marker, markerRadius * 0.33f, gameplay.skin.playerCore);
    if (gameplay.showHitFlash) {
        locus::app::DrawPlayerHitFlash(
            marker, markerRadius, gameplay.hitFlash.Sample(now), gameplay.skin);
    }
    EndScissorMode();

    // The world camera and its square remain unchanged. Anchor the HUD
    // to the full visible window, including narrow and portrait layouts.
    const Rectangle visible = transform.ToVirtualBounds(
        {0.0f,0.0f,static_cast<float>(GetScreenWidth()),
                   static_cast<float>(GetScreenHeight())});
    DrawGameplayFieldEdges(kGameplayWorldViewport,visible,gameplay.skin.fieldBorder);
    const auto layout = LayoutGameplayHud(visible,kGameplayWorldViewport);
    DrawGameplayHud(font,layout,MakeGameplayHudStats(gameplay.judge,gameplay.practiceSpeed,
                                                     gameplay.watchingReplay),
                    gameplay.skin,&gameplay.hitFlash.Errors(),now);
    if (now < gameplay.adjustmentNoticeUntil)
        DrawGameplayNotice(font,layout,gameplay.adjustmentNotice,gameplay.skin);
}

void DrawResultsScreen(Font font, const GameplayView& gameplay,
                       const RunResult* saved, bool historical) {
    const auto& theme = ApprovedUiTheme();
    const core::GradeCounts& counts =
        saved ? saved->grades : gameplay.judge.Counts();
    const std::string title = saved ? saved->chartTitle : gameplay.title;
    const Color muted = theme.secondaryText;
    const Color sky = theme.softBlue;
    const Color violet = theme.rose;

    DrawRectangleRounded({72, 77, 412, 306}, 0.04f, 8,
                         Color{38, 43, 66, 246});
    DrawRectangleRounded({497, 77, 398, 306}, 0.04f, 8,
                         Color{39, 45, 68, 246});
    DrawTextLabel(font, historical ? "SAVED RESULT" :
                       gameplay.completed ? "CHART COMPLETE" : "RESULTS",
                  Vector2{94.0f, 92.0f}, 28.0f, sky);
    const std::string shown = core::utf8::Truncate(title, 36);
    DrawTextLabel(font, shown.c_str(), {94, 130}, 17, theme.foreground);
    if (saved) {
        const auto date = FormatRunTimestamp(saved->completedAtMs);
        DrawTextLabel(font, date.c_str(), {94, 158}, 13, muted);
    }
    DrawTextLabel(font, "ACCURACY", {94, 206}, 17, muted);
    DrawTextLabel(font, TextFormat("%.2f%%", counts.AccuracyPercent()),
                  {94, 232}, 44, violet);
    const auto rank=core::RankForAccuracy(
        saved ? saved->accuracyPercent : counts.AccuracyPercent(),counts.Resolved());
    const Color rankColor=ResultRankColor(rank);
    const char* rankText=core::ResultRankName(rank);
    const float rankSize=80.0f;
    const auto rankWidth=MeasureTextEx(font,rankText,rankSize,1.0f).x;
    // The fallback renderer has no RCSS font effects. Keep the rank in a
    // separate area of the summary instead of overlapping practice labels.
    DrawTextLabel(font,rankText,{(72+412)-rankWidth-24,288},rankSize,rankColor);
    if (saved && saved->variablePracticeRate)
        DrawTextLabel(font,
                      TextFormat("VARIABLE RATE / LAST %.0f%%",
                                 saved->practiceRate * 100),
                      {94, 346}, 13, sky);
    else if (saved && saved->practiceRate < 0.999)
        DrawTextLabel(font,
                      TextFormat("PRACTICE %.0f%%", saved->practiceRate * 100),
                      {94, 346}, 13, sky);

    const auto item = [&](const char* name, int value, float y, Color tint) {
        DrawTextLabel(font, name, {523, y}, 19, tint);
        DrawTextLabel(font, TextFormat("%d", value), {794, y}, 20,
                      theme.foreground);
    };
    item("PERFECT", counts.perfect, 112, sky);
    item("GOOD", counts.good, 176, violet);
    item("BAD", counts.bad, 240, muted);
    item("MISS", counts.miss, 304, Color{210, 164, 203, 255});
    if (!historical) {
        DrawUiThemedButton(font, kResultsReplayButton, "PLAY AGAIN",
                           UiButtonVariant::Blue);
    }
}


} // namespace locus::app
