#pragma once

#include "app/skin.h"
#include "app/hit_error_history.h"
#include "core/judgement.h"
#include "raylib.h"

#include <algorithm>

namespace locus::app {

// Layout uses the actual world clip and visible window. Detailed grades
// never occupy gameplay pixels, even on narrow windows.
struct GameplayHudLayout {
    Rectangle bounds{};
    float unit = 1.0f;
    Rectangle combo{}, accuracy{}, grades{}, notice{}, hitError{};
};

inline GameplayHudLayout LayoutGameplayHud(Rectangle bounds,
                                           Rectangle field) noexcept {
    GameplayHudLayout out{};
    out.bounds = bounds;
    if (bounds.width <= 0 || bounds.height <= 0) return out;
    const float u = std::min(bounds.width, bounds.height) / 540.0f;
    out.unit = u;
    const float margin = 16.0f*u;
    const float left = bounds.x + margin;
    const float right = bounds.x + bounds.width - margin;
    const float top = bounds.y + margin;
    const float bottom = bounds.y + bounds.height - margin;
    const float fieldRight = field.x + field.width;
    const float rightGutter = bounds.x + bounds.width - fieldRight;
    const float leftGutter = field.x - bounds.x;

    // Prefer outside gutters for the two main values too. In square and
    // portrait windows these essential values stay in the screen corners.
    const auto cornerWidth = [&](float gutter) {
        return gutter >= 116.0f*u
            ? std::min(200.0f*u, gutter-2.0f*margin)
            : 176.0f*u;
    };
    const float comboWidth = cornerWidth(leftGutter);
    const float accuracyWidth = cornerWidth(rightGutter);
    out.combo = {left, bottom-45.0f*u, comboWidth, 45.0f*u};
    out.accuracy = {right-accuracyWidth, top, accuracyWidth, 60.0f*u};

    // Only use the right gutter, with a minimum readable width.
    constexpr float kGradeWidth = 100.0f;
    constexpr float kGradeHeight = 152.0f;
    if (rightGutter >= (kGradeWidth+32.0f)*u) {
        const float width = std::min(170.0f*u, rightGutter-2.0f*margin);
        out.grades = {fieldRight+(rightGutter-width)*0.5f,
                      bounds.y+(bounds.height-kGradeHeight*u)*0.5f,
                      width, kGradeHeight*u};
    }
    // A narrow, optional bottom-center timing bar. Keep it clear of the
    // combo corner in square/portrait layouts.
    const float barWidth = 144.0f*u;
    out.hitError = {bounds.x+(bounds.width-barWidth)*0.5f,
                    bottom-18.0f*u,barWidth,18.0f*u};
    if (out.hitError.x < out.combo.x+out.combo.width+4.0f*u)
        out.hitError.y = out.combo.y-22.0f*u;
    out.notice = {bounds.x+(bounds.width-260.0f*u)*0.5f,
                  top+66.0f*u,260.0f*u,28.0f*u};
    return out;
}

inline Rectangle LayoutHitErrorBar(const GameplayHudLayout& l,const Skin& skin) noexcept {
    const float u=l.unit;
    const auto b=l.bounds;
    const float w=std::min(skin.hitErrorLength*u,std::max(0.0f,b.width-8*u));
    Rectangle bar{b.x+(b.width-w)*0.5f,b.y+b.height-34*u,w,18*u};
    if (bar.x < l.combo.x+l.combo.width+4*u && bar.x+bar.width > l.combo.x)
        bar.y=l.combo.y-22*u;
    return bar;
}

struct GameplayHudStats {
    core::GradeCounts grades{};
    int combo = 0;
    double practiceRate = 1.0;
    bool watchingReplay = false;
    core::ModFlags modFlags = 0;
};

inline GameplayHudStats MakeGameplayHudStats(const core::TurnJudge& judge,
                                              double practiceRate,
                                              bool watchingReplay = false) noexcept {
    return {judge.Counts(), judge.Combo(), practiceRate, watchingReplay, judge.Mods()};
}

void DrawGameplayHud(Font font, const GameplayHudLayout& layout,
                     const GameplayHudStats& stats, const Skin& skin,
                     const HitErrorHistory* errors = nullptr, double now = 0.0);
void DrawGameplayNotice(Font font, const GameplayHudLayout& layout,
                        const char* notice, const Skin& skin);
void DrawGameplayFieldEdges(Rectangle field, Rectangle visible, Color tint);

} // namespace locus::app
