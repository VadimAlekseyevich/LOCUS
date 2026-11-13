#include "app/gameplay_ui.h"
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

void DrawPlayGrid(Font font, const GameplayView& gameplay, double now) {
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
    Vector2 marker = locus::app::DrawTimedRouteTrajectory(
        preview, canvas, gameplay.skin);
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
        DrawLineEx(marker,
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

    // Draw the frame AFTER all clipped route/player primitives. Alpha is
    // supplied by the skin and the thin line remains inside the viewport.
    DrawRectangleLinesEx(
        {kGameplayWorldViewport.x + 0.75f, kGameplayWorldViewport.y + 0.75f,
         kGameplayWorldViewport.width - 1.5f,
         kGameplayWorldViewport.height - 1.5f},
        1.5f, gameplay.skin.fieldBorder);

    // Keep the entire LEFT side empty. In non-16:9 windows, the virtual
    // 960px canvas is cropped at both edges: the old LOCUS / LAST HIT /
    // SONG / SQUARE labels were literally cut in half on the player's
    // screen. Those labels are no longer part of scored gameplay.
    // Display speed and preview below the stats, or a brief notice inside
    // the square when no HUD is available (including portrait windows).
    const float sideWidth = GameplayRightSidebarWidth(
        GetScreenWidth(), GetScreenHeight());
    if (!gameplay.skin.showHud || sideWidth <= 0.0f) {
        if (now < gameplay.adjustmentNoticeUntil) {
            const char* value = gameplay.adjustmentNotice;
            const float size = 17.0f;
            const Vector2 measured = MeasureTextEx(font, value, size, 1.0f);
            const Rectangle panel{
                kGameplayWorldViewport.x +
                    (kGameplayWorldViewport.width - measured.x) * 0.5f - 12.0f,
                17.0f, measured.x + 24.0f, 32.0f};
            DrawRectangleRounded(panel, 0.2f, 4,
                                 locus::app::WithOpacity(gameplay.skin.hudBackground, 0.87f));
            DrawTextLabel(font, value, {panel.x + 12.0f, panel.y + 7.0f},
                          size, gameplay.skin.hudText);
        }
        return;
    }

    const float squareRight = kGameplayWorldViewport.x +
                              kGameplayWorldViewport.width;
    DrawRectangleRec({squareRight, 0.0f, sideWidth,
                      static_cast<float>(kVirtualHeight)},
                     gameplay.skin.hudBackground);

    const float padding = std::min(24.0f, sideWidth * 0.10f);
    const float right = squareRight + padding;
    const float availableTextWidth = sideWidth - padding - 8.0f;
    if (availableTextWidth < 20.0f) return;

    // Fit each heading/value within the truly visible right sidebar
    // rather than blindly drawing at positions outside the window.
    const auto hudText = [&](const char* label, float y, float desiredSize,
                             Color color) {
        const float naturalWidth = MeasureTextEx(
            font, label, desiredSize, 1.0f).x;
        const float fitted = naturalWidth > availableTextWidth
            ? desiredSize * availableTextWidth / naturalWidth
            : desiredSize;
        DrawTextLabel(font, label, {right, y}, fitted, color);
    };

    const auto& counts = gameplay.judge.Counts();
    hudText("ACCURACY", 30.0f, 17.0f, gameplay.skin.hudText);
    hudText(TextFormat("%.2f%%", counts.AccuracyPercent()),
            63.0f, 30.0f, gameplay.skin.hudText);
    DrawLineEx({right, 118.0f},
               {squareRight + sideWidth - 8.0f, 118.0f}, 1.0f,
               locus::app::WithOpacity(gameplay.skin.hudText, 0.28f));
    const struct {
        const char* label;
        int count;
        Color color;
    } stats[] = {
        {"PERFECT", counts.perfect, {133, 236, 201, 255}},
        {"GOOD", counts.good, {143, 208, 239, 255}},
        {"BAD", counts.bad, {242, 198, 135, 255}},
        {"MISS", counts.miss, {237, 142, 149, 255}}
    };
    for (int i = 0; i < 4; ++i) {
        const float y = 144.0f + i * 89.0f;
        hudText(stats[i].label, y, 17.0f, stats[i].color);
        hudText(TextFormat("%d", stats[i].count), y + 26.0f,
                28.0f, gameplay.skin.hudText);
    }
    hudText(TextFormat("PREVIEW %.0f MS", gameplay.skin.lookaheadMs),
            480.0f, 14.0f, gameplay.skin.hudText);
    hudText(TextFormat("SPEED %.0f PX/S", gameplay.skin.baseSpeedPxPerSecond),
            502.0f, 15.0f, gameplay.skin.hudText);
}

void DrawResultsScreen(Font font, const GameplayView& gameplay,
                       const RunResult* saved, bool historical) {
    const auto& theme = ApprovedUiTheme();
    const core::GradeCounts& counts =
        saved ? saved->grades : gameplay.judge.Counts();
    const int total = saved ? saved->notesJudged
        : static_cast<int>(gameplay.judge.ScorableCount());
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
    DrawTextLabel(font, TextFormat("%d / %d NOTES JUDGED",
                  counts.Resolved(), total),
                  {94, 319}, 15, muted);
    if (saved && saved->practiceRate < 0.999)
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
        DrawUiThemedButton(font, kResultsEditorButton, "EDIT CHART",
                           UiButtonVariant::Rose);
    }
    DrawUiThemedButton(font, kResultsLibraryButton, "SONG LIBRARY",
                       UiButtonVariant::Neutral);
}


} // namespace locus::app
