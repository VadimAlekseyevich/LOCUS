#include "app/chart_editor_ui.h"
#include "core/tempo_map.h"
#include "app/editor_comparison.h"
#include "core/utf8.h"
#include "app/chart_editor_layout.h"
#include "core/judgement.h"
#include "app/route_renderer.h"
#include "app/viewport.h"
#include "app/ui_button.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace locus::app {
using namespace detail;
namespace {

void Label(Font font, const char* value, float x, float y, float size, Color color) {
    DrawTextEx(font, value, {x, y}, size, 1.0f, color);
}

std::string Truncate(const std::string& value, std::size_t maxCharacters) {
    return core::utf8::Truncate(value, maxCharacters);
}

void Button(Font font, Rectangle rect, const char* text, Color tint, bool active = false) {
    DrawUiButton(font, rect, text, EditorButtonStyle(tint), active);
}

// ROUTE toolbar controls are shorter and less visually dominant than notes.
// Hitboxes are exactly the rectangles from chart_editor_layout.h.
void CompactButton(Font font, Rectangle rect, const char* text, Color tint,
                   bool active = false) {
    UiButtonStyle style = EditorButtonStyle(tint);
    style.fontSize = rect.height <= 26.0f ? 12.0f : 14.0f;
    style.fontSpacing = 0.5f;
    style.strokeWidth = 1.1f;
    style.selectedStrokeWidth = 1.5f;
    DrawUiButton(font, rect, text, style, active);
}

// The compass pad authors direction notes by clicking, never by keybinding.
void DirectionButton(Font font, Rectangle rect, const char* count,
                     Color tint, bool active = false) {
    DrawUiButton(font, rect, count, EditorButtonStyle(tint), active);
}

// Vector icons, so Windows' Latin/Cyrillic font needs no arrow glyphs.
enum class EditorIcon { FlipH, FlipV, RotateL, RotateR, Compare, Play, Pause, Restart, Previous, Next };
void EditorGlyph(Rectangle bounds, EditorIcon icon, Color tint) {
    const float x = bounds.x + bounds.width / 2;
    const float y = bounds.y + bounds.height / 2;
    const auto line = [&](float ax, float ay, float bx, float by) {
        DrawLineEx({x+ax,y+ay},{x+bx,y+by},2.5f,tint);
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
            DrawRing({x,y},8,10,-70,210,28,tint);
            if (icon == EditorIcon::RotateL)
                DrawTriangle({x-12,y-10},{x-4,y-12},{x-8,y-3},tint);
            else
                DrawTriangle({x+12,y-10},{x+8,y-3},{x+4,y-12},tint);
            break;
        case EditorIcon::Compare:
            DrawRectangleRoundedLinesEx({x-13,y-12,18,20},.2f,6,2,tint);
            DrawRectangleRoundedLinesEx({x-4,y-5,18,20},.2f,6,2,tint); break;
        case EditorIcon::Play:
            DrawTriangle({x-7,y-11},{x-7,y+11},{x+11,y},tint); break;
        case EditorIcon::Pause:
            DrawRectangleRec({x-9,y-10,6,20},tint);
            DrawRectangleRec({x+3,y-10,6,20},tint); break;
        case EditorIcon::Restart:
            DrawRing({x,y},9,11,-50,245,28,tint);
            DrawTriangle({x-12,y-10},{x-4,y-10},{x-9,y-2},tint); break;
        case EditorIcon::Previous:
            line(6,-9,-5,0);line(-5,0,6,9);break;
        case EditorIcon::Next:
            line(-6,-9,5,0);line(5,0,-6,9);break;
    }
}
void EditorIconButton(Rectangle bounds, EditorIcon icon,
                      Color color, bool selected=false) {
    auto style = EditorButtonStyle(color);
    style.roundness = UiRoundness(bounds,9);
    DrawUiButtonChrome(bounds,style,selected);
    EditorGlyph(bounds,icon,color);
}
void HeadingArrow(Rectangle bounds, core::Heading heading, Color tint) {
    constexpr float dx[]{0,1,0,-1,1,1,-1,-1};
    constexpr float dy[]{-1,0,1,0,-1,1,1,-1};
    const auto i=static_cast<int>(heading);
    if(i<0||i>=8)return;
    const float scale=core::IsDiagonal(heading)?.7071f:1.0f;
    const float x=dx[i]*scale, y=dy[i]*scale;
    const Vector2 center{bounds.x+bounds.width/2,bounds.y+bounds.height*.45f};
    const Vector2 end{center.x+x*13,center.y+y*13};
    DrawLineEx({center.x-x*10,center.y-y*10},end,2.7f,tint);
    DrawLineEx(end,{end.x-x*6-y*6,end.y-y*6+x*6},2.4f,tint);
    DrawLineEx(end,{end.x-x*6+y*6,end.y-y*6-x*6},2.4f,tint);
}
void EditorToolTip(Font font, Rectangle button, const std::string& caption) {
    const Vector2 mouse = EditorVirtualMouse(
        GetMousePosition(),GetScreenWidth(),GetScreenHeight());
    if (!Hit(button,mouse)) return;
    const float width=MeasureTextEx(font,caption.c_str(),12,1).x+22;
    const Rectangle popup{button.x+button.width+9,button.y+6,width,29};
    DrawRectangleRounded(popup,UiRoundness(popup,7),6,Color{40,45,68,248});
    DrawRectangleRoundedLinesEx(popup,UiRoundness(popup,7),6,1,
                                ApprovedUiTheme().rose);
    Label(font,caption.c_str(),popup.x+11,popup.y+7,12,
          ApprovedUiTheme().foreground);
}

const char* HeadingName(core::Heading heading) {
    switch (heading) {
        case core::Heading::Up: return "UP";
        case core::Heading::Right: return "RIGHT";
        case core::Heading::Down: return "DOWN";
        case core::Heading::Left: return "LEFT";
        case core::Heading::UpRight: return "UP+RIGHT";
        case core::Heading::DownRight: return "DOWN+RIGHT";
        case core::Heading::DownLeft: return "DOWN+LEFT";
        case core::Heading::UpLeft: return "UP+LEFT";
    }
    return "?";
}

void DrawTextField(Font font, Rectangle rect, const char* label,
                   const std::string& value, EditorField field,
                   const EditorUiState& ui) {
    Label(font, label, rect.x, rect.y - 21.0f, 16.0f, Color{172, 199, 218, 255});
    const bool active = ui.active == field;
    DrawRectangleRounded(rect, 0.14f, 5,
                         active ? Color{47, 68, 78, 255}
                                : Color{35, 43, 56, 255});
    DrawRectangleRoundedLinesEx(rect, 0.14f, 5, active ? 2.0f : 1.0f,
                                active ? Color{129, 235, 214, 255}
                                       : Color{91, 114, 139, 255});
    const std::string shown = Truncate(active ? ui.buffer : value, field == EditorField::Title
                                                                      ? 26 : 19);
    Label(font, shown.c_str(), rect.x + 10.0f, rect.y + 9.0f, 19.0f,
          Color{225, 235, 241, 255});
    if (active) {
        const float cursor = MeasureTextEx(font, shown.c_str(), 19, 1.0f).x;
        const float x = std::min(rect.x + rect.width - 6.0f, rect.x + 12.0f + cursor);
        DrawLineEx({x, rect.y + 9.0f}, {x, rect.y + 31.0f},
                   1.6f, Color{186, 251, 235, 255});
    }
}

void DrawComparisonPreviews(Font font, const EditorComparisonFrame& comparison,
                            double songSeconds, float pixelsPerSongSecond,
                            const Skin& skin) {
    const int count = std::clamp(comparison.count, 1, 3);
    for (int index = 0; index < count; ++index) {
        const auto& pane = comparison.panes[static_cast<std::size_t>(index)];
        const Rectangle bounds = ComparisonPreviewBounds(count, index);
        DrawRectangleRounded(bounds, 0.05f, 5, Color{23, 30, 43, 242});
        const Color accent = pane.editable
            ? Color{111, 221, 198, 255}
            : Color{141, 157, 195, 255};
        DrawRectangleRoundedLinesEx(bounds, 0.05f, 5,
                                    pane.editable ? 2.0f : 1.0f, accent);
        if (pane.route && pane.route->Ready()) {
            // The cache prepares snapshots before rendering. The local
            // fallback is only for standalone editor preview callers.
            RoutePreviewFrame fallback;
            if (!pane.preview)
                PrepareRoutePreview(*pane.route, songSeconds,
                                    skin.lookaheadMs, fallback);
            const RoutePreviewFrame& preview = pane.preview
                ? *pane.preview : fallback;
            // One visual px/s scale across every pane, regardless of BPM,
            // authoring snap/grid, note density or panel dimensions.
            const float step = pixelsPerSongSecond /
                static_cast<float>(pane.route->UnitsPerSecond());
            const RouteCanvas canvas{
                bounds.x + bounds.width * 0.5f,
                bounds.y + bounds.height * 0.5f,
                step, preview.marker};
            // Scissor coordinates are physical pixels, not virtual UI units.
            const auto clip = ViewportTransform::Menu(
                GetScreenWidth(), GetScreenHeight()).Scissor(bounds);
            BeginScissorMode(clip.x, clip.y, clip.width, clip.height);
            const Vector2 marker = DrawTimedRouteTrajectory(
                preview, canvas, skin);
            DrawTimedRouteJunctions(preview, canvas, skin);
            const float radius = std::min(10.0f, bounds.width * 0.04f);
            DrawCircleV(marker, radius * 1.65f,
                        Color{112, 219, 216, 48});
            DrawCircleV(marker, radius, Color{124, 229, 225, 255});
            DrawCircleV(marker, radius * 0.34f,
                        Color{246, 251, 252, 255});
            EndScissorMode();
        }
        // Draw a short identifier rather than prose across the playing field.
        const int maxCharacters = std::max(6, static_cast<int>(
            (bounds.width - (pane.editable ? 95.0f : 12.0f)) / 7.6f));
        const std::string title = (pane.editable
            ? (pane.dirty ? "EDIT* " : "EDIT ")
            : (pane.dirty ? "VIEW* " : "VIEW ")) +
            Truncate(pane.title, static_cast<std::size_t>(maxCharacters - 5));
        Label(font, title.c_str(), bounds.x + 5.0f, bounds.y + 5.0f,
              bounds.width < 155.0f ? 10.0f : 12.0f, accent);
    }

    // Time now lives in the shared bottom overview, not the route field.

}

void DrawComparisonPicker(Font font, const EditorUiState& ui,
                          const EditorSetView& set) {
    if (!ui.comparisonPickerOpen) return;
    DrawRectangleRounded(kComparePopup, 0.08f, 6,
                         Color{21, 29, 44, 253});
    DrawRectangleRoundedLinesEx(kComparePopup, 0.08f, 6, 1.5f,
                                Color{118, 208, 198, 255});
    Label(font, "COMPARE / SAME SONG", kComparePopup.x + 10,
          kComparePopup.y + 6, 12.0f, Color{209, 234, 232, 255});
    const int count = static_cast<int>(set.comparisonOptions.size());
    for (int row = 0; row < 5; ++row) {
        const int index = ui.comparisonPickerFirst + row;
        if (index >= count) break;
        const auto& option = set.comparisonOptions[
            static_cast<std::size_t>(index)];
        const bool editable = index == set.current;
        const bool checked = HasEditorComparison(ui, option.file);
        const Rectangle rect = ComparePickerRow(row);
        const std::string label =
            (editable ? "EDIT  " : checked ? "[-]  " : "[+]  ") +
            Truncate(option.title, 23);
        CompactButton(font, rect, label.c_str(),
            editable ? Color{110, 219, 194, 255}
                     : checked ? Color{116, 217, 204, 255}
                               : Color{164, 186, 224, 255},
            editable || checked);
    }
    CompactButton(font, kComparePickerPrev, "PREV", Color{148, 163, 193, 255});
    CompactButton(font, kComparePickerNext, "NEXT", Color{148, 163, 193, 255});
    Label(font, TextFormat("%d / %d",
        std::min(count, ui.comparisonPickerFirst + 5), count),
        kComparePopup.x + 107, kComparePopup.y + 182, 11.0f, Color{183, 202, 216, 255});
}

// Zoomable, note-bearing ruler. Its pixel scale never controls the
// full-song overview or the compiled song-time route.
void DrawTimeline(Font font, const core::ChartEditor& draft,
                  const EditorUiState& ui, double previewBeat) {
    const auto& chart = draft.Data().route;
    const double anchor = ui.draggingTimeline
        ? ui.dragViewBeat : ViewBeat(draft, previewBeat);
    const float pixels = PixelsPerTick(ui, chart);
    const int ticks = chart.ticksPerBeat;
    const int snapStep = ticks / chart.snapDivision;
    const Rectangle strip{kTimelineX, kTimelineY, kTimelineWidth, kTimelineHeight};
    const float lineY = kTimelineY + 25.0f;

    DrawRectangleRounded(strip, UiRoundness(strip,8), 8,
                         Color{30, 37, 58, 255});
    DrawRectangleRoundedLinesEx(strip,UiRoundness(strip,8),8,1,
                                ApprovedUiTheme().divider);
    DrawRectangleRec({kTimelineX + 2.0f, kTimelineY + 2.0f,
                      kPlayheadX - kTimelineX - 2.0f, kTimelineHeight - 4.0f},
                     Color{41, 62, 70, 255});
    if (ui.tab == EditorTab::Route && ui.HasRange()) {
        const float a = kPlayheadX +
            static_cast<float>((ui.rangeFirst - snapStep * 0.5 - anchor) * pixels);
        const float b = kPlayheadX +
            static_cast<float>((ui.rangeLast + snapStep * 0.5 - anchor) * pixels);
        const float x1 = std::max(a, kTimelineX + 2.0f);
        const float x2 = std::min(b, kTimelineX + kTimelineWidth - 2.0f);
        if (x2 > x1)
            DrawRectangleRec({x1, kTimelineY + 2.0f, x2 - x1,
                              kTimelineHeight - 4.0f}, Color{187, 160, 231, 96});
    }
    DrawLineEx({kTimelineX + 4.0f, lineY},
               {kTimelineX + kTimelineWidth - 4.0f, lineY},
               2.0f, Color{98, 125, 150, 255});

    const int first = std::max(chart.firstBeat, static_cast<int>(std::floor(
        anchor + (kTimelineX - kPlayheadX) / pixels)) - 1);
    const int last = std::min(chart.lengthBeats - 1,
        static_cast<int>(std::ceil(
            anchor + (kTimelineX + kTimelineWidth - kPlayheadX) / pixels)) + 1);
    for (int tick = first; tick <= last; ++tick) {
        if (tick % snapStep != 0) continue;
        const float x = kPlayheadX +
                        static_cast<float>((static_cast<double>(tick) - anchor) * pixels);
        if (x < kTimelineX + 5.0f || x > kTimelineX + kTimelineWidth - 5.0f)
            continue;
        const bool bar = tick % (4 * ticks) == 0;
        const bool major = tick % ticks == 0;
        DrawLineEx({x, kTimelineY + (bar ? 11.0f : major ? 17.0f : 23.0f)},
                   {x, kTimelineY + 34.0f},
                   bar ? 2.5f : major ? 1.5f : 1.0f,
                   bar ? Color{199, 220, 234, 255}
                     : major ? Color{113, 141, 163, 255}
                             : Color{81, 126, 135, 220});
    }

    // The authored chart can contain 65k notes, but only a few fit in the
    // zoomed ruler. Find the visible range in O(log N), not by scanning every
    // previous event on each rendered frame.
    const auto firstVisible = std::lower_bound(
        chart.turns.begin(), chart.turns.end(), first,
        [](const core::TurnEvent& event, int tick) {
            return event.beat < tick;
        });
    for (auto it = firstVisible;
         it != chart.turns.end() && it->beat <= last; ++it) {
        const auto& event = *it;
        const float x = kPlayheadX +
                        static_cast<float>((static_cast<double>(event.beat) - anchor) * pixels);
        if (x < kTimelineX + 8.0f || x > kTimelineX + kTimelineWidth - 8.0f)
            continue;
        const bool diagonal = core::IsDiagonal(event.direction);
        const float spacing = pixels * snapStep;
        const bool narrow = spacing < 26.0f;
        const float width = narrow ? std::max(5.0f, std::min(spacing - 1.0f, 12.0f))
                                   : std::min(spacing - 3.0f, 32.0f);
        const Color tint = diagonal ? Color{175, 155, 236, 255}
                                    : Color{111, 214, 202, 255};
        DrawRectangleRec({x - width * 0.5f, kTimelineY + 29.0f, width, 12.0f}, tint);
        if (!narrow) {
            constexpr const char* names[] = {
                "U", "R", "D", "L", "UR", "DR", "DL", "UL"
            };
            const char* label = names[static_cast<int>(event.direction)];
            const float textWidth = MeasureTextEx(font, label, 10, 1).x;
            Label(font, label, x - textWidth * 0.5f, kTimelineY + 29.0f, 10,
                  Color{18, 26, 36, 255});
        }
    }
    DrawRectangleRec({kPlayheadX - 3.0f, kTimelineY + 1.0f,
                      6.0f, kTimelineHeight - 2.0f},
                     ApprovedUiTheme().softBlue);
    DrawCircleV({kPlayheadX, lineY}, 5.0f, ApprovedUiTheme().foreground);

    // The note ruler and the 2x +/- pairs share a single upper row.
    if (ui.tab == EditorTab::Route) {
        const auto& colors = ApprovedUiTheme();
        Label(font,TextFormat("SNAP 1/%d",chart.snapDivision),
              730,121,12,colors.softBlue);
        Label(font,TextFormat("ZOOM %d%%",
              static_cast<int>(std::round(PixelsPerBeat(ui)/36.0f*100))),
              730,146,12,colors.secondaryText);
        CompactButton(font,kSnapLess,"-",colors.rose);
        CompactButton(font,kSnapMore,"+",colors.softBlue);
        CompactButton(font,kZoomOut,"-",colors.rose);
        CompactButton(font,kZoomIn,"+",colors.softBlue);
    }

}

void DrawSongOverview(Font font, const core::ChartEditor& draft,
                      const EditorSetView& set, double songSeconds) {
    const Rectangle bounds = kSongOverview;
    DrawRectangleRounded(bounds,UiRoundness(bounds,8),8,Color{30,38,60,255});
    DrawRectangleRoundedLinesEx(bounds,UiRoundness(bounds,8),8,1,
                                ApprovedUiTheme().divider);
    const double duration = draft.SongDurationSeconds();
    if (!std::isfinite(duration) || duration <= 0.0) {
        Label(font, "NO AUDIO", bounds.x + 9.0f, bounds.y + 8.0f,
              12.0f, Color{136, 158, 178, 255});
        return;
    }

    const float right = bounds.x + bounds.width;
    const float cursor = SongOverviewX(songSeconds, duration);
    const float lineY = bounds.y + 25.0f;
    if (cursor > bounds.x + 2.0f) {
        DrawRectangleRec({bounds.x + 2.0f, lineY - 2.0f,
                          cursor - bounds.x - 2.0f, 5.0f},
                         Color{98,102,161,120});
    }
    DrawLineEx({bounds.x + 3.0f, lineY}, {right - 3.0f, lineY},
               1.5f, Color{104, 134, 154, 255});

    // This is the pack's shared tempo/beat-zero anchor, not a chart note.
    // Additional tempo events can later be supplied through this collection,
    // without changing full-song navigation or the note ruler.
    const auto tempoMarkers = set.managed && !set.tempoMap.empty()
        ? set.tempoMap
        : core::EffectiveTempoMap(draft.Data().route, draft.Data().offsetSeconds);
    float previousTextRight = -1000.0f;
    for (const auto& marker : tempoMarkers) {
        const float x = SongOverviewX(marker.timeMs / 1000.0, duration);
        DrawLineEx({x, bounds.y + 5.0f}, {x, bounds.y + 31.0f},
                   2.0f, Color{128, 223, 210, 255});
        const char* name = TextFormat("%g BPM", marker.bpm);
        const float width = MeasureTextEx(font, name, 11.0f, 1.0f).x;
        const float labelX = std::clamp(x + 4.0f, bounds.x + 7.0f,
            std::max(bounds.x + 7.0f, right - width - 6.0f));
        if (labelX > previousTextRight + 6.0f) {
            Label(font, name, labelX, bounds.y + 3.0f, 11.0f,
                  Color{172, 233, 219, 255});
            previousTextRight = labelX + width;
        }
    }

    // Song-time cursor is distinct from the anchored, moving upper ruler.
    DrawRectangleRec({cursor - 2.0f, bounds.y + 2.0f,
                      4.0f, bounds.height - 4.0f},
                     Color{241, 206, 118, 255});
    DrawCircleV({cursor, lineY}, 4.0f, ApprovedUiTheme().foreground);
    const int total = static_cast<int>(std::ceil(duration));
    const char* end = TextFormat("%d:%02d", total / 60, total % 60);
    const float timeWidth = MeasureTextEx(font, end, 11.0f, 1.0f).x;
    Label(font, end, right - timeWidth - 7.0f, bounds.y + 3.0f, 11.0f,
          Color{170, 191, 211, 255});
}

} // namespace

void RenderChartEditor(Font font, const core::ChartEditor& draft,
                       const EditorUiState& ui, const std::string& songName,
                       bool hasAudio, bool previewPlaying, bool previewPaused,
                       double previewBeat, double playbackSpeed,
                       const GameSettings& settings, const Skin& skin,
                       const EditorSetView& set,
                       const EditorComparisonFrame* comparisons,
                       double previewSeconds, bool nativeShell) {
    const auto& chart = draft.Data().route;
    const auto& theme = ApprovedUiTheme();
    const bool running = previewPlaying && !previewPaused;
    const bool routeTab = ui.tab == EditorTab::Route;
    const Color violet = theme.rose;
    const Color sky = theme.softBlue;
    const Color muted = theme.secondaryText;

    // Native editor shell replaces ONLY common chrome, not authoring views.
    if (!nativeShell) {
    DrawRectangleRounded({43,65,899,45},.12f,8,theme.surface);
    DrawUiThemedButton(font,kEditorBack,"BACK",UiButtonVariant::Quiet,
                       {},{},theme,14);
    const std::string activeTitle = Truncate(draft.Data().title,23);
    Label(font,activeTitle.c_str(),160,80,16,theme.foreground);
    if (draft.Dirty()) Label(font,"*",385,79,19,violet);
    DrawUiThemedButton(font,kTabSet,"SET",
                       ui.tab==EditorTab::Set?UiButtonVariant::Cool:UiButtonVariant::Quiet);
    DrawUiThemedButton(font,kTabRoute,"ROUTE",
                       routeTab?UiButtonVariant::Cool:UiButtonVariant::Quiet);
    DrawUiThemedButton(font,kTabAudio,"AUDIO",
                       ui.tab==EditorTab::Audio?UiButtonVariant::Cool:UiButtonVariant::Quiet);
    }

    if (routeTab) {
        // Unlike the old horizontal button bars, this six-icon vertical
        // rail never reduces the usable height of the playing field.
        EditorIconButton(kMirrorHorizontal,EditorIcon::FlipH,violet);
        EditorIconButton(kMirrorVertical,EditorIcon::FlipV,violet);
        EditorIconButton(kRotateCounterclockwise,EditorIcon::RotateL,sky);
        EditorIconButton(kRotateClockwise,EditorIcon::RotateR,sky);
        EditorIconButton(kCompareToggle,EditorIcon::Compare,
                         set.managed?sky:muted,ui.comparisonPickerOpen);
        EditorIconButton(kRouteTest,EditorIcon::Play,sky);

        const Rectangle previewArea{
            kPreviewLeft-3,kPreviewTop-3,
            kPreviewRight-kPreviewLeft+6,kPreviewMaxSide+6};
        DrawRectangleRounded(previewArea,UiRoundness(previewArea,12),10,
                             Color{25,33,53,235});
        DrawRectangleRoundedLinesEx(previewArea,UiRoundness(previewArea,12),
                                    10,1,theme.divider);
        EditorComparisonFrame fallback;
        core::TimedRoute one;
        if (!comparisons) {
            if (one.Compile(draft.Data().route,draft.Data().offsetSeconds))
                fallback.panes[0]={&one,draft.Data().title,true};
            fallback.count=1;
            fallback.directionPressCounts=DirectionPressCounts(chart);
            comparisons=&fallback;
        }
        Skin editorSkin=skin;
        editorSkin.lookaheadMs=settings.editorPreviewLengthMs;
        DrawComparisonPreviews(font,*comparisons,previewSeconds,
            settings.editorPreviewSpeed/
                static_cast<float>(std::max(0.1,playbackSpeed)),editorSkin);
        const int focused=std::clamp(ui.comparisonFocusedSlot,0,
                                    comparisons->count-1);
        const Rectangle focusedBounds=ComparisonPreviewBounds(
            comparisons->count,focused);
        Label(font,TextFormat("%.1f",draft.Difficulty().overall),
              focusedBounds.x+focusedBounds.width-43,
              focusedBounds.y+23,12,sky);

        // Only the compass buttons place notes; diagonal counts also
        // contribute to their corresponding cardinal arrows.
        const auto* turn=draft.SelectedTurn();
        const core::Heading incoming=core::IncomingHeadingAt(
            chart,draft.SelectedBeat());
        const core::Heading outgoing=turn?turn->direction:incoming;
        const auto& counts=comparisons->directionPressCounts;
        for (int row=0;row<3;++row) for(int col=0;col<3;++col) {
            const auto heading=DirectionPadHeading(row,col);
            const Rectangle rect=DirectionPadBounds(row,col);
            auto style=EditorButtonStyle(
                heading&&core::IsDiagonal(*heading)?violet:sky);
            style.roundness=UiRoundness(rect,9);
            style.fill=Color{45,53,80,245};
            DrawUiButtonChrome(rect,style,
                heading&&turn&&*heading==outgoing);
            if (!heading) {
                const char* del="DEL";
                const float w=MeasureTextEx(font,del,14,1).x;
                Label(font,del,rect.x+(rect.width-w)*.5f,
                      rect.y+20,14,muted);
            } else {
                HeadingArrow(rect,*heading,
                             core::IsDiagonal(*heading)?violet:sky);
                const std::string count=std::to_string(
                    counts[static_cast<std::size_t>(*heading)]);
                Label(font,count.c_str(),rect.x+rect.width-
                      MeasureTextEx(font,count.c_str(),10,1).x-5,
                      rect.y+rect.height-15,10,muted);
            }
        }
        Label(font,"START",750,377,12,muted);
        EditorIconButton(kStartHeadingPrev,EditorIcon::Previous,sky);
        EditorIconButton(kStartHeadingNext,EditorIcon::Next,sky);
        const Rectangle start=kStartHeadingValue;
        DrawRectangleRounded(start,UiRoundness(start,8),8,theme.surface);
        DrawRectangleRoundedLinesEx(start,UiRoundness(start,8),8,1,violet);
        HeadingArrow(start,chart.initialHeading,violet);

        Label(font,"SPEED",749,410,12,muted);
        Label(font,TextFormat("%.0f px/s",settings.editorPreviewSpeed),
              749,427,11,sky);
        CompactButton(font,kEditorSpeedMinus,"-",violet);
        CompactButton(font,kEditorSpeedPlus,"+",sky);
        Label(font,"PATH",749,446,12,muted);
        Label(font,TextFormat("%.0f ms",settings.editorPreviewLengthMs),
              749,462,11,sky);
        CompactButton(font,kEditorPathMinus,"-",violet);
        CompactButton(font,kEditorPathPlus,"+",sky);
    } else if (ui.tab == EditorTab::Audio) {
        DrawRectangleRounded(kAudioMetaPanel,
            UiRoundness(kAudioMetaPanel,12),10,theme.surface);
        DrawRectangleRoundedLinesEx(kAudioMetaPanel,
            UiRoundness(kAudioMetaPanel,12),10,1,theme.divider);
        DrawRectangleRounded(kAudioTempoPanel,
            UiRoundness(kAudioTempoPanel,12),10,theme.surface);
        DrawRectangleRoundedLinesEx(kAudioTempoPanel,
            UiRoundness(kAudioTempoPanel,12),10,1,theme.divider);
        Label(font,"CHART",66,185,17,violet);
        DrawTextField(font,kTitle,"DIFFICULTY TITLE",
                      draft.Data().title,EditorField::Title,ui);
        Label(font,"SONG PREVIEW START  /  MS",kAudioPreviewMs.x,
              kAudioPreviewMs.y-21,13,muted);
        const Color box=ui.editingSetPreview
            ? Color{61,58,91,255}:Color{34,42,64,255};
        DrawRectangleRounded(kAudioPreviewMs,
            UiRoundness(kAudioPreviewMs,9),9,box);
        DrawRectangleRoundedLinesEx(kAudioPreviewMs,
            UiRoundness(kAudioPreviewMs,9),9,
            ui.editingSetPreview?2.0f:1.0f,
            ui.editingSetPreview?sky:theme.divider);
        const std::string preview = ui.editingSetPreview
            ? ui.setPreviewBuffer
            : (set.managed ? std::to_string(set.previewStartMs)
                           : "SAVE SONG FIRST");
        Label(font,preview.c_str(),kAudioPreviewMs.x+10,
              kAudioPreviewMs.y+10,15,theme.foreground);
        Label(font,TextFormat("DIFFICULTY  %.1f",
              draft.Difficulty().overall),
              66,372,16,sky);
        DrawUiThemedButton(font,kSong,
            hasAudio?"REPLACE AUDIO":"ATTACH AUDIO",
            UiButtonVariant::Quiet,{}, {},theme,13);

        const auto changes=set.managed&&!set.tempoMap.empty()
            ?set.tempoMap
            :core::EffectiveTempoMap(chart,draft.Data().offsetSeconds);
        Label(font,"SONG TEMPO MAP",384,186,17,violet);
        CompactButton(font,kTempoPrev,"PREV",violet);
        CompactButton(font,kTempoNext,"NEXT",sky);
        Label(font,"OFFSET / MS",384,220,12,muted);
        Label(font,"BPM",590,220,12,muted);
        const int count=static_cast<int>(changes.size());
        const int first=std::clamp(ui.tempoFirstVisible,0,
                                   std::max(0,count-5));
        for(int row=0;row<5;++row) {
            const int index=first+row;
            if(index>=count)break;
            const auto& marker=changes[static_cast<std::size_t>(index)];
            const auto cell=[&](Rectangle bounds,const std::string& value,
                                bool editing) {
                const auto tint=editing?sky:theme.divider;
                DrawRectangleRounded(bounds,UiRoundness(bounds,7),8,
                   editing?Color{62,59,92,255}:Color{31,41,63,255});
                DrawRectangleRoundedLinesEx(bounds,
                   UiRoundness(bounds,7),8,editing?2.0f:1.0f,tint);
                Label(font,UiFitText(font,value,bounds.width-16,14).c_str(),
                      bounds.x+8,bounds.y+8,14,theme.foreground);
            };
            char bpm[48]{};
            std::snprintf(bpm,sizeof(bpm),"%g",marker.bpm);
            const bool msActive=ui.tempoEditingRow==index&&
                                !ui.tempoEditingBpm;
            const bool bpmActive=ui.tempoEditingRow==index&&
                                 ui.tempoEditingBpm;
            cell(TempoOffsetCell(row),
                msActive?ui.tempoBuffer:std::to_string(marker.timeMs),msActive);
            cell(TempoBpmCell(row),
                bpmActive?ui.tempoBuffer:std::string(bpm),bpmActive);
            if(index>0)CompactButton(font,TempoDeleteCell(row),
                                    "DELETE",violet);
            else Label(font,"START",TempoDeleteCell(row).x+20,
                       TempoDeleteCell(row).y+9,12,sky);
        }
        DrawUiThemedButton(font,kTempoAdd,"+ ADD BPM MARKER",
                           UiButtonVariant::Cool,{}, {},theme,13);
        Label(font,TextFormat("%d / %d",count?first+1:0,count),
              801,437,12,muted);
    } else if (ui.tab == EditorTab::Set) {
        const Rectangle metadataPanel{43,175,522,294};
        const Rectangle difficultiesPanel{567,175,370,294};
        DrawRectangleRounded(metadataPanel,UiRoundness(metadataPanel,11),
                             9,theme.surface);
        DrawRectangleRoundedLinesEx(metadataPanel,UiRoundness(metadataPanel,11),
                                    9,1,theme.divider);
        DrawRectangleRounded(difficultiesPanel,UiRoundness(difficultiesPanel,11),
                             9,theme.surface);
        DrawRectangleRoundedLinesEx(difficultiesPanel,
                                    UiRoundness(difficultiesPanel,11),
                                    9,1,theme.divider);
        // Same geometry and hitboxes as SET input. Only this tab changes:
        // shared Song Set metadata is editable, chart files remain separate.
        const auto& theme = ApprovedUiTheme();
        const Color sky = theme.softBlue;
        const Color violet = theme.rose;
        const Color normal = theme.foreground;
        const Color muted = theme.secondaryText;
        const auto metadataField = [&](EditorSetField field,
                                       Rectangle bounds, const char* label,
                                       const std::string& value) {
            const bool focus = ui.editingSetField == field;
            Label(font, label, bounds.x, bounds.y - 15, 12, muted);
            DrawRectangleRounded(bounds, 0.16f, 6,
                focus ? Color{60, 59, 94, 255} : theme.surface);
            DrawRectangleRoundedLinesEx(bounds, 0.16f, 6,
                focus ? 2.0f : 1.0f,
                focus ? sky : Color{112, 112, 158, 160});
            const std::string& text = focus ? ui.setFieldBuffer : value;
            const int limit = static_cast<int>(bounds.width / 9.5f) - 2;
            const std::string shown = Truncate(text,
                static_cast<std::size_t>(std::max(4, limit)));
            Label(font, shown.c_str(), bounds.x + 9, bounds.y + 8,
                  15, set.managed ? normal : muted);
            if (focus) {
                const float width = MeasureTextEx(font, shown.c_str(),
                                                  15, 1.0f).x;
                const float x = std::min(bounds.x + bounds.width - 5,
                                           bounds.x + 11 + width);
                DrawLineEx({x, bounds.y + 7}, {x, bounds.y + 26},
                           1.6f, sky);
            }
        };
        std::string tagList;
        for (const auto& tag : set.tags) {
            if (!tagList.empty()) tagList += ", ";
            tagList += tag;
        }
        Label(font,"SONG SET",53,164,12,theme.rose);
        Label(font,"DIFFICULTIES",575,164,12,theme.softBlue);
        metadataField(EditorSetField::Title, kSetTitleField,
                      "SONG TITLE", set.title);
        metadataField(EditorSetField::Artist, kSetArtistField,
                      "ARTIST", set.artist);
        metadataField(EditorSetField::Mapper, kSetMapperField,
                      "MAPPER", set.mapper);
        metadataField(EditorSetField::Tags, kSetTagsField,
                      "TAGS", tagList);
        if (set.managed) {
            CompactButton(font, kSetBackground,
                set.hasBackground ? "BACKGROUND" : "ADD BACKGROUND", sky);
            CompactButton(font, kSetRemoveBackground, "REMOVE BG", violet);
            CompactButton(font, kSetOpenFolder, "OPEN FOLDER", sky);
        }
        CompactButton(font, kSetImport, "IMPORT", sky);
        CompactButton(font, kSetExport, "EXPORT", violet);
        CompactButton(font, kSetRestore, "RESTORE", violet);
        CompactButton(font, kSetNew, "NEW", sky);
        CompactButton(font, kSetDuplicate, "DUPLICATE", violet);
        CompactButton(font, kSetDeleteDifficulty,
                      "DELETE CHART", Color{214, 159, 198, 255});
        CompactButton(font, kSetDeleteSong,
                      "DELETE SET", Color{214, 159, 198, 255});

        for (int row = 0; row < 5; ++row) {
            const int index = set.firstVisible + row;
            if (index >= static_cast<int>(set.difficulties.size())) break;
            const Rectangle bounds = SetDifficultyRow(row);
            const bool chosen = index == set.selected;
            const bool active = index == set.current;
            DrawRectangleRounded(bounds, 0.13f, 6,
                chosen ? Color{62, 61, 95, 255} : theme.surface);
            DrawRectangleRoundedLinesEx(bounds, 0.13f, 6,
                active ? 2.0f : 1.0f, active ? sky : theme.divider);
            const std::string title = Truncate(set.difficulties[
                static_cast<std::size_t>(index)], 29);
            Label(font, title.c_str(), bounds.x + 11,
                  bounds.y + 8, 15, normal);
            if (active) Label(font, "EDITING", bounds.x + 282,
                              bounds.y + 10, 11, violet);
        }
        if (!set.managed)
            Label(font, "SAVE CHART TO ENABLE PACK METADATA",
                  53, 463, 10, muted);
    } else {
        Label(font, "INITIAL HEADING", 104, 207, 17,
              Color{194, 218, 231, 255});
        Button(font, kFieldHeadingPrev, "<", Color{145, 189, 221, 255});
        Button(font, kFieldHeading, HeadingName(chart.initialHeading),
               Color{119, 229, 188, 255});
        Button(font, kFieldHeadingNext, ">", Color{145, 189, 221, 255});
        Label(font, TextFormat("SPEED %d PX/S", 
              static_cast<int>(skin.baseSpeedPxPerSecond)),
              104, 329, 16, Color{169, 210, 218, 255});
        Label(font, TextFormat("PREVIEW %d MS",
              static_cast<int>(skin.lookaheadMs)),
              383, 329, 16, Color{169, 210, 218, 255});

    }

    DrawTimeline(font,draft,ui,previewBeat);
    DrawSongOverview(font,draft,set,previewSeconds);

    // Timeline and song overview still use raylib; transport chrome is RmlUi.
    if (!nativeShell) {
    EditorIconButton(kEditorPreviewPlay,
        running?EditorIcon::Pause:EditorIcon::Play,sky);
    EditorIconButton(kEditorPreviewRestart,EditorIcon::Restart,violet);
    const char* speeds[5]={"100%","75%","50%","25%","10%"};
    int rate=0;
    for(int i=0;i<5;++i)
        if(std::abs(playbackSpeed-kSpeeds[i])<.001)rate=i;
    DrawUiThemedButton(font,kEditorRateDropdown,
        (std::string(speeds[rate])+" v").c_str(),
        UiButtonVariant::Quiet,{}, {},theme,13);
    DrawUiThemedButton(font,kRouteTimeValue,
        TextFormat("%lld ms",RouteTimeMs(draft,previewBeat)),
        UiButtonVariant::Quiet,{}, {},theme,12);
    }

    if(routeTab) {
        DrawComparisonPicker(font,ui,set);
        // Hover hint includes configured shortcuts, not hardcoded defaults.
        EditorToolTip(font,kMirrorHorizontal,
            "Flip horizontally  /  "+
            KeyboardChordName(settings.Shortcut(ShortcutAction::EditorMirrorH)));
        EditorToolTip(font,kMirrorVertical,
            "Flip vertically  /  "+
            KeyboardChordName(settings.Shortcut(ShortcutAction::EditorMirrorV)));
        EditorToolTip(font,kRotateCounterclockwise,"Rotate 90 left");
        EditorToolTip(font,kRotateClockwise,"Rotate 90 right");
        EditorToolTip(font,kCompareToggle,"Compare charts");
        EditorToolTip(font,kRouteTest,
            "Test chart  /  "+
            KeyboardChordName(settings.Shortcut(ShortcutAction::EditorTest)));
    }
    if(ui.rateDropdownOpen && !nativeShell) {
        DrawRectangleRounded(kEditorRatePopup,UiRoundness(kEditorRatePopup,9),
                             8,Color{32,38,63,254});
        DrawRectangleRoundedLinesEx(kEditorRatePopup,
            UiRoundness(kEditorRatePopup,9),8,1,theme.rose);
        for(int i=0;i<5;++i)
            DrawUiThemedButton(font,EditorRateOption(i),speeds[i],
               i==rate?UiButtonVariant::Cool:UiButtonVariant::Quiet,
               {},{},theme,12);
    }

}

} // namespace locus::app
