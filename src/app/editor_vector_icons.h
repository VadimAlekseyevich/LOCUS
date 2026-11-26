#pragma once
#include "core/route.h"
#include "raylib.h"
#include <cmath>

namespace locus::app {

// The same glyph geometry is used by the legacy raylib authoring UI and the
// RmlUi editor chrome; do not rely on host font glyph coverage for directions.
enum class EditorIcon { FlipH, FlipV, RotateL, RotateR, Compare, Play, Pause, Restart, Stop, Test, Previous, Next };

inline void EditorGlyph(Rectangle bounds, EditorIcon icon, Color tint,
                        float uiScale = 1.0f) {
    const float s = uiScale;
    const float x = bounds.x + bounds.width * 0.5f;
    const float y = bounds.y + bounds.height * 0.5f;
    const auto pt = [&](float dx, float dy) -> Vector2 {
        return {x + dx * s, y + dy * s};
    };
    const auto line = [&](float ax, float ay, float bx, float by) {
        DrawLineEx(pt(ax,ay), pt(bx,by), 2.5f*s, tint);
    };
    switch (icon) {
        case EditorIcon::FlipH:
            line(0,-12,0,12); line(-13,0,13,0);
            line(-13,0,-6,-6); line(-13,0,-6,6);
            line(13,0,6,-6); line(13,0,6,6); break;
        case EditorIcon::FlipV:
            line(-12,0,12,0); line(0,-13,0,13);
            line(0,-13,-6,-6); line(0,-13,6,-6);
            line(0,13,-6,6); line(0,13,6,6); break;
        case EditorIcon::RotateL:
        case EditorIcon::RotateR:
            DrawRing({x,y},8*s,10*s,-70,210,28,tint);
            if (icon == EditorIcon::RotateL)
                DrawTriangle(pt(-12,-10),pt(-4,-12),pt(-8,-3),tint);
            else
                DrawTriangle(pt(12,-10),pt(8,-3),pt(4,-12),tint);
            break;
        case EditorIcon::Compare:
            line(-12,-5,12,-5); line(12,-5,6,-11); line(12,-5,6,1);
            line(12,5,-12,5); line(-12,5,-6,-1); line(-12,5,-6,11); break;
        case EditorIcon::Test:
            DrawRing({x-3*s,y-3*s},6*s,8.5f*s,0,360,32,tint);
            line(3,3,11,11); break;
        case EditorIcon::Stop:
            DrawRectangleRec({x-8*s,y-8*s,16*s,16*s},tint); break;
        case EditorIcon::Play:
            DrawTriangle(pt(-7,-11),pt(-7,11),pt(11,0),tint); break;
        case EditorIcon::Pause:
            DrawRectangleRec({x-9*s,y-10*s,6*s,20*s},tint);
            DrawRectangleRec({x+3*s,y-10*s,6*s,20*s},tint); break;
        case EditorIcon::Restart:
            DrawRing({x,y},9*s,11*s,-50,245,28,tint);
            DrawTriangle(pt(-12,-10),pt(-4,-10),pt(-9,-2),tint); break;
        case EditorIcon::Previous:
            line(6,-9,-5,0); line(-5,0,6,9); break;
        case EditorIcon::Next:
            line(-6,-9,5,0); line(5,0,-6,9); break;
    }
}

inline void HeadingArrow(Rectangle bounds, core::Heading heading, Color tint,
                         float uiScale = 1.0f) {
    constexpr float dx[]{0,1,0,-1,1,1,-1,-1};
    constexpr float dy[]{-1,0,1,0,-1,1,1,-1};
    const auto index = static_cast<int>(heading);
    if (index < 0 || index >= 8) return;
    const float diagonal = core::IsDiagonal(heading) ? .7071f : 1.0f;
    const float vx = dx[index]*diagonal, vy = dy[index]*diagonal;
    const float s = uiScale;
    const Vector2 center{bounds.x+bounds.width*.5f,
                         bounds.y+bounds.height*.45f};
    const Vector2 end{center.x+vx*13*s,center.y+vy*13*s};
    DrawLineEx({center.x-vx*10*s,center.y-vy*10*s},end,2.7f*s,tint);
    DrawLineEx(end,{end.x+(-vx-vy)*6*s,end.y+(-vy+vx)*6*s},2.4f*s,tint);
    DrawLineEx(end,{end.x+(-vx+vy)*6*s,end.y+(-vy-vx)*6*s},2.4f*s,tint);
}

} // namespace locus::app
