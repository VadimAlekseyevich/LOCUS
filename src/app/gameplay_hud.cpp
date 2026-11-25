#include "app/gameplay_hud.h"

#include "app/visual_style.h"

#include <cstdio>

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

void Scrims(const GameplayHudLayout& l, const Skin& skin) {
    const auto b = l.bounds;
    const float height = 110.0f*l.unit;
    const Color top = WithOpacity(skin.hudBackground, 0.82f);
    const Color clear = WithOpacity(skin.hudBackground, 0.0f);
    DrawRectangleGradientEx({b.x,b.y,b.width,height},top,clear,clear,top);
    DrawRectangleGradientEx({b.x,b.y+b.height-height,b.width,height},
                            clear,top,top,clear);
}

} // namespace

void DrawGameplayHud(Font font, const GameplayHudLayout& l,
                     const GameplayHudStats& stats, const Skin& skin) {
    if (!skin.showHud || l.bounds.width <= 0 || l.bounds.height <= 0) return;
    const float u = l.unit;
    const Color text = skin.hudText;
    const Color muted = WithOpacity(text, 0.69f);
    Scrims(l, skin);
    char value[64];
    std::snprintf(value, sizeof(value), "%dx", stats.combo);
    Text(font,value,l.combo,0,34,u,text);
    std::snprintf(value, sizeof(value), "Best %dx", stats.maxCombo);
    Text(font,value,l.combo,41,14,u,muted);

    std::snprintf(value, sizeof(value), "%.2f%%", stats.grades.AccuracyPercent());
    Text(font,value,l.accuracy,0,34,u,text,Align::Right);
    Text(font,"Accuracy",l.accuracy,41,14,u,muted,Align::Right);
    if (stats.practiceRate < 0.999 || stats.practiceRate > 1.001) {
        std::snprintf(value, sizeof(value), "Practice  %.0f%%", stats.practiceRate*100.0);
        Text(font,value,l.accuracy,60,12,u,muted,Align::Right);
    }

    const int counts[] = {stats.grades.perfect, stats.grades.good,
                          stats.grades.bad, stats.grades.miss};
    const char* labels[] = {"Perfect","Good","Bad","Miss"};
    const float cell = l.grades.width / 4.0f;
    for (int i = 0; i < 4; ++i) {
        const Rectangle slot{l.grades.x+i*cell,l.grades.y,
                             cell-5.0f*u,l.grades.height};
        const Color accent = WithOpacity(kGradeColors[i],text.a/255.0f);
        std::snprintf(value,sizeof(value),"%d",counts[i]);
        Text(font,value,slot,0,22,u,accent);
        Text(font,labels[i],slot,27,12,u,muted);
    }

    std::snprintf(value,sizeof(value),"%d / %d notes",stats.grades.Resolved(),stats.total);
    Text(font,value,l.progress,0,16,u,text,Align::Right);
    if (!stats.last) std::snprintf(value,sizeof(value),"Last hit  --");
    else if (stats.last->grade == core::Grade::Miss)
        std::snprintf(value,sizeof(value),"%s",
                      stats.last->wrongDirection ? "Wrong direction" : "Missed");
    else {
        const double ms = stats.last->timingErrorSeconds*1000.0;
        std::snprintf(value,sizeof(value),"Last hit  %+.0f ms",ms);
    }
    Text(font,value,l.progress,22,12,u,muted,Align::Right);

    // The thin bar represents resolved physical notes, not audio elapsed.
    const auto b = l.bounds;
    const float barHeight = 3.0f*u;
    const Rectangle track{b.x,b.y+b.height-barHeight,b.width,barHeight};
    DrawRectangleRec(track,WithOpacity(text,0.12f));
    if (const float progress=JudgedProgress(stats); progress>0.0f)
        DrawRectangleRec({track.x,track.y,track.width*progress,track.height},
                         WithOpacity(text,0.8f));
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
