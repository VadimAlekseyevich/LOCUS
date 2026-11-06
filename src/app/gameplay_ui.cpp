#include "app/gameplay_ui.h"

#include "app/visual_style.h"

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
    const GameplayCamera camera(gameplay.chart, gameplay.beat, gameplay.skin,
                                gameplay.practiceSpeed);
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

    // PLAY and editor TEST: only a gradually thinning/fading red route
    // *ahead* of the marker, red turn targets and the centred avatar.
    // No past-route trail, grid or debug overlay.
    const Vector2 marker = locus::app::DrawRouteTrajectory(
        gameplay.chart, gameplay.beat, canvas,
        locus::app::RouteRenderMode::Gameplay, gameplay.skin);
    locus::app::DrawRouteJunctions(
        gameplay.chart, gameplay.beat, canvas,
        locus::app::RouteRenderMode::Gameplay, gameplay.skin);
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
    // Display speed below the stats, or briefly in the square when no HUD
    // can be displayed (including portrait windows).
    const float sideWidth = GameplayRightSidebarWidth(
        GetScreenWidth(), GetScreenHeight());
    if (!gameplay.skin.showHud || sideWidth <= 0.0f) {
        if (now < gameplay.speedNoticeUntil) {
            const char* value = TextFormat("SPEED %.0f PX/S",
                                          gameplay.skin.baseSpeedPxPerSecond);
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
    hudText(TextFormat("SPEED %.0f PX/S", gameplay.skin.baseSpeedPxPerSecond),
            502.0f, 15.0f, gameplay.skin.hudText);
}

void DrawResultsScreen(Font font, const GameplayView& gameplay) {
    const auto& judge = gameplay.judge;
    const auto& counts = judge.Counts();
    DrawTextLabel(font, gameplay.completed
                            ? (gameplay.custom ? "CHART COMPLETE" : "DEMO COMPLETE")
                            : "RESULTS PREVIEW",
                  Vector2{100.0f, 111.0f}, 41.0f,
                  Color{195, 237, 232, 255});
    const std::string title = gameplay.title.substr(0, 29);
    DrawTextLabel(font, title.c_str(), Vector2{570.0f, 167.0f},
                  17.0f, Color{170, 215, 222, 255});
    DrawTextLabel(font, TextFormat("PERFECT   %d", counts.perfect),
                  Vector2{110.0f, 206.0f}, 27.0f,
                  Color{128, 238, 204, 255});
    DrawTextLabel(font, TextFormat("GOOD      %d", counts.good),
                  Vector2{110.0f, 250.0f}, 27.0f,
                  Color{147, 215, 245, 255});
    DrawTextLabel(font, TextFormat("BAD       %d", counts.bad),
                  Vector2{110.0f, 294.0f}, 27.0f,
                  Color{240, 197, 138, 255});
    DrawTextLabel(font, TextFormat("MISS      %d", counts.miss),
                  Vector2{110.0f, 338.0f}, 27.0f,
                  Color{243, 150, 151, 255});

    DrawTextLabel(font, "ACCURACY", Vector2{570.0f, 215.0f}, 22.0f,
                  Color{170, 215, 222, 255});
    DrawTextLabel(font, TextFormat("%.2f%%", counts.AccuracyPercent()),
                  Vector2{570.0f, 245.0f}, 43.0f,
                  Color{225, 246, 229, 255});
    DrawTextLabel(font,
                  TextFormat("NOTES JUDGED  %d / %d", counts.Resolved(),
                             static_cast<int>(judge.ScorableCount())),
                  Vector2{570.0f, 327.0f}, 19.0f,
                  Color{204, 225, 228, 255});
    DrawTextLabel(font, "NO FAIL  -  ROUTE UNCHANGED",
                  Vector2{570.0f, 367.0f}, 15.0f,
                  Color{158, 193, 204, 255});
    DrawTextLabel(font, "ENTER / R  -  PLAY AGAIN",
                  Vector2{110.0f, 409.0f}, 24.0f,
                  Color{236, 219, 160, 255});
    DrawTextLabel(font, "H  -  HOME",
                  Vector2{110.0f, 447.0f}, 22.0f,
                  Color{187, 205, 220, 255});
}


} // namespace locus::app
