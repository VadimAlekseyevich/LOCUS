#pragma once

#include "app/skin.h"
#include "core/judgement.h"
#include "raylib.h"

#include <algorithm>

namespace locus::app {

// Coordinates are local to the current draw transform, not the legacy
// 960x540 menu canvas. The short edge controls density; the long edge
// contributes actual space instead of scaling down the entire HUD.
struct GameplayHudLayout {
    Rectangle bounds{};
    float unit = 1.0f;
    Rectangle combo{}, accuracy{}, grades{}, progress{}, notice{};
};

inline GameplayHudLayout LayoutGameplayHud(Rectangle bounds) noexcept {
    GameplayHudLayout out{};
    out.bounds = bounds;
    if (bounds.width <= 0 || bounds.height <= 0) return out;
    const float u = std::min(bounds.width, bounds.height) / 540.0f;
    out.unit = u;
    const float left = bounds.x + 20.0f*u;
    const float right = bounds.x + bounds.width - 20.0f*u;
    const float top = bounds.y + 15.0f*u;
    const float bottom = bounds.y + bounds.height - 20.0f*u;
    out.combo = {left, top, 200.0f*u, 82.0f*u};
    out.accuracy = {right - 200.0f*u, top, 200.0f*u, 82.0f*u};
    out.grades = {left, bottom - 52.0f*u, 252.0f*u, 52.0f*u};
    out.progress = {right - 210.0f*u, bottom - 52.0f*u, 210.0f*u, 52.0f*u};
    out.notice = {bounds.x + (bounds.width-260.0f*u)*0.5f,
                  top + 90.0f*u, 260.0f*u, 28.0f*u};
    return out;
}

struct GameplayHudStats {
    core::GradeCounts grades{};
    int total = 0;
    int combo = 0;
    int maxCombo = 0;
    std::optional<core::Judgement> last;
    double practiceRate = 1.0;
};

inline GameplayHudStats MakeGameplayHudStats(const core::TurnJudge& judge,
                                               double practiceRate) noexcept {
    return {judge.Counts(), static_cast<int>(judge.ScorableCount()),
            judge.Combo(), judge.MaxCombo(), judge.Last(), practiceRate};
}

inline float JudgedProgress(const GameplayHudStats& stats) noexcept {
    return stats.total > 0
        ? std::clamp(static_cast<float>(stats.grades.Resolved()) / stats.total,
                     0.0f, 1.0f) : 0.0f;
}

void DrawGameplayHud(Font font, const GameplayHudLayout& layout,
                     const GameplayHudStats& stats, const Skin& skin);
void DrawGameplayNotice(Font font, const GameplayHudLayout& layout,
                        const char* notice, const Skin& skin);
void DrawGameplayFieldEdges(Rectangle field, Rectangle visible, Color tint);

} // namespace locus::app
