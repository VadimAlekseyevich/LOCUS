#include "app/gameplay_hud.h"

#include "app/visual_style.h"

#include <cstdio>
#include <cmath>

namespace locus::app {
namespace {

enum class Align { Left, Right, Center };

void Text(Font font, const char* value, Rectangle slot, float y, float size,
          float unit, Color color, Align align = Align::Left) {
    float fontSize = size * unit;
    float spacing = 0.7f * unit;
    float width = MeasureTextEx(font, value, fontSize, spacing).x;
    if (width > slot.width) {
        const float fit = slot.width / width;
        fontSize *= fit;
        spacing *= fit;
        width = MeasureTextEx(font, value, fontSize, spacing).x;
    }
    const float x = align == Align::Right ? slot.x + slot.width - width
        : align == Align::Center ? slot.x + (slot.width-width)*0.5f : slot.x;
    DrawTextEx(font, value, {x, slot.y + y*unit}, fontSize, spacing, color);
}

constexpr Color kGradeColors[] = {
    {142, 226, 204, 255}, {155, 205, 245, 255},
    {237, 198, 143, 255}, {239, 149, 168, 255}
};

void DrawHitErrorBar(const GameplayHudLayout& l, const Skin& skin,
                     const HitErrorHistory* errors, double now, double windowScale) {
    const auto b = LayoutHitErrorBar(l,skin);
    if (b.width<=0) return;
    const float u = l.unit;
    const float center = b.x+b.width*0.5f;
    const auto xFor = [&](double error) {
        return center + b.width*0.5f*static_cast<float>(
            std::clamp(error/(core::kBadWindowSeconds*windowScale),-1.0,1.0));
    };
    const float y = b.y+b.height*0.5f;
    // One skin color; subtle opacity differences preserve timing zones.
    const auto range = [&](double from, double to, float opacity) {
        const float x=xFor(from*windowScale);
        DrawRectangleRec({x,y-2*u,xFor(to*windowScale)-x,4*u},
                         WithOpacity(skin.hitErrorColor,opacity));
    };
    range(-core::kBadWindowSeconds,-core::kGoodWindowSeconds,0.18f);
    range(-core::kGoodWindowSeconds,-core::kPerfectWindowSeconds,0.32f);
    range(-core::kPerfectWindowSeconds,core::kPerfectWindowSeconds,0.55f);
    range(core::kPerfectWindowSeconds,core::kGoodWindowSeconds,0.32f);
    range(core::kGoodWindowSeconds,core::kBadWindowSeconds,0.18f);
    DrawRectangleRec({center-u,y-7*u,2*u,14*u},skin.hitErrorColor);
    if (!errors) return;
    for (std::size_t i=0; i<errors->Size(); ++i) {
        const auto& sample=errors->At(i);
        const float opacity=HitErrorHistory::Opacity(sample,now);
        if (opacity<=0.0f) continue;
        const float x=xFor(sample.errorSeconds);
        const auto c=skin.hitErrorColor;
        DrawRectangleRec({x-u,y-6.0f*u,2.0f*u,12.0f*u},
                         WithOpacity(c,opacity));
    }
}

} // namespace

void DrawGameplayHud(Font font, const GameplayHudLayout& l,
                     const GameplayHudStats& stats, const Skin& skin,
                     const HitErrorHistory* errors, double now) {
    if (l.bounds.width <= 0 || l.bounds.height <= 0) return;
    if (stats.watchingReplay) {
        const Rectangle label{l.combo.x,l.bounds.y+16.0f*l.unit,120.0f*l.unit,20.0f*l.unit};
        Text(font,"REPLAY",label,0,15,l.unit,skin.hudText);
    }
    if (!skin.showHud) return;
    const float u = l.unit;
    const Color text = skin.hudText;
    const Color muted = WithOpacity(text, 0.69f);
    char value[64];
    std::snprintf(value, sizeof(value), "%dx", stats.combo);
    Text(font,value,l.combo,0,34,u,text);

    std::snprintf(value, sizeof(value), "%.2f%%", stats.grades.AccuracyPercent());
    Text(font,value,l.accuracy,0,34,u,text,Align::Right);
    if (stats.modFlags || stats.practiceRate < 0.999 || stats.practiceRate > 1.001) {
        const auto mods = core::FormatMods(static_cast<int>(std::lround(stats.practiceRate*100)),stats.modFlags);
        Text(font,mods.c_str(),l.accuracy,41,12,u,muted,Align::Right);
    }

    if (skin.showHitErrorBar) DrawHitErrorBar(l,skin,errors,now,core::WindowScale(stats.modFlags));
    if (l.grades.width <= 0.0f) return;
    const int counts[] = {stats.grades.perfect, stats.grades.good,
                          stats.grades.bad, stats.grades.miss};
    const char* labels[] = {"Perfect","Good","Bad","Miss"};
    const float rowHeight = l.grades.height / 4.0f;
    for (int i = 0; i < 4; ++i) {
        const float y = l.grades.y+i*rowHeight;
        const float labelWidth = 0.52f*l.grades.width;
        const Rectangle label{l.grades.x,y,labelWidth,rowHeight};
        const Rectangle number{l.grades.x+labelWidth+4.0f*u,y,
                               l.grades.width-labelWidth-4.0f*u,rowHeight};
        const Color accent = WithOpacity(kGradeColors[i],text.a/255.0f);
        Text(font,labels[i],label,6,14,u,muted);
        std::snprintf(value,sizeof(value),"%d",counts[i]);
        Text(font,value,number,3,21,u,accent,Align::Right);
    }
}

void DrawGameplayNotice(Font font, const GameplayHudLayout& l,
                        const char* notice, const Skin& skin) {
    if (!notice || !*notice || l.bounds.width <= 0 || l.bounds.height <= 0) return;
    const auto b = l.notice;
    const float u = l.unit;
    DrawRectangleRounded(b,0.5f,8,WithOpacity(skin.hudBackground,0.85f));
    const Rectangle content{b.x+8*u,b.y,b.width-16*u,b.height};
    Text(font,notice,content,6,14,u,skin.hudText,Align::Center);
}

// Quiet lateral guides indicate clipping without enclosing the scene.
void DrawGameplayFieldEdges(Rectangle field, Rectangle visible, Color tint) {
    const float u = std::min(visible.width, visible.height)/540.0f;
    if (u <= 0.0f) return;
    const Color clear = WithOpacity(tint,0.0f);
    const Color soft = WithOpacity(tint,0.18f);
    const float thickness = 1.0f*u;
    const auto edge = [&](float x) {
        const float half = field.height*0.5f;
        DrawRectangleGradientEx({x,field.y,thickness,half},
                                clear,soft,soft,clear);
        DrawRectangleGradientEx({x,field.y+half,thickness,half},
                                soft,clear,clear,soft);
    };
    if (field.x > visible.x+thickness) edge(field.x-thickness);
    if (field.x+field.width < visible.x+visible.width-thickness)
        edge(field.x+field.width);
}

} // namespace locus::app
