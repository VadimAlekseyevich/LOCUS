#include "app/chart_editor_ui.h"
#include "core/audio_waveform.h"
#include "app/path_mesh.h"
#include "core/tempo_map.h"
#include "app/editor_comparison.h"
#include "core/utf8.h"
#include "app/chart_editor_layout.h"
#include "core/judgement.h"
#include "app/route_renderer.h"
#include "app/viewport.h"
#include "app/ui_button.h"
#include "app/editor_vector_icons.h"

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

void EditorIconButton(Rectangle bounds, EditorIcon icon,
                      Color color, bool selected=false) {
    if (!HasLayoutArea(bounds)) return;
    auto style = EditorButtonStyle(color);
    style.roundness = UiRoundness(bounds,9);
    DrawUiButtonChrome(bounds,style,selected);
    EditorGlyph(bounds,icon,color);
}
void EditorToolTip(Font font, const EditorUiState& ui, Rectangle button,
                   const std::string& caption) {
    const Vector2 mouse = ui.layout.Coordinates(
        GetScreenWidth(),GetScreenHeight()).ToVirtual(GetMousePosition());
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

void DrawComparisonPreviews(Font font, const EditorUiState& ui,
                            const EditorComparisonFrame& comparison,
                            double songSeconds, float pixelsPerSongSecond,
                            const Skin& skin,PathRibbonWorkspace& ribbon) {
    const int count = std::clamp(comparison.count, 1, 3);
    for (int index = 0; index < count; ++index) {
        const auto& pane = comparison.panes[static_cast<std::size_t>(index)];
        const Rectangle bounds = ComparisonPreviewBounds(ui, count, index);
        if (!HasLayoutArea(bounds)) continue;
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
            const auto clip = ui.layout.Coordinates(
                GetScreenWidth(), GetScreenHeight()).Scissor(bounds);
            BeginScissorMode(clip.x, clip.y, clip.width, clip.height);
            const Vector2 marker = DrawTimedRouteTrajectory(
                preview,canvas,skin,ribbon);
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
    const auto popup = ComparePopupBounds(ui);
    if (!HasLayoutArea(popup)) return;
    DrawRectangleRounded(popup, 0.08f, 6,
                         Color{21, 29, 44, 253});
    DrawRectangleRoundedLinesEx(popup, 0.08f, 6, 1.5f,
                                Color{118, 208, 198, 255});
    Label(font, "COMPARE / SAME SONG", popup.x + 10,
          popup.y + 6, 12.0f, Color{209, 234, 232, 255});
    const int count = static_cast<int>(set.comparisonOptions.size());
    for (int row = 0; row < 5; ++row) {
        const int index = ui.comparisonPickerFirst + row;
        if (index >= count) break;
        const auto& option = set.comparisonOptions[
            static_cast<std::size_t>(index)];
        const bool editable = index == set.current;
        const bool checked = HasEditorComparison(ui, option.file);
        const Rectangle rect = ComparePickerRow(ui, row);
        const std::string label =
            (editable ? "EDIT  " : checked ? "[-]  " : "[+]  ") +
            Truncate(option.title, 23);
        CompactButton(font, rect, label.c_str(),
            editable ? Color{110, 219, 194, 255}
                     : checked ? Color{116, 217, 204, 255}
                               : Color{164, 186, 224, 255},
            editable || checked);
    }
    if (count > 5)
        Label(font, "SCROLL", popup.x + 10, popup.y + 182,
              11.0f, Color{183, 202, 216, 255});
    Label(font, TextFormat("%d / %d",
        std::min(count, ui.comparisonPickerFirst + 5), count),
        popup.x + 107, popup.y + 182, 11.0f, Color{183, 202, 216, 255});
}

// Zoomable, note-bearing ruler. Its pixel scale never controls the
// full-song overview or the compiled song-time route.
// Time-linear, note-bearing ruler. Source milliseconds are authoritative.
// Each tempo marker starts its own beat grid at a strong beat; increasing BPM
// packs lines more tightly instead of changing the ruler's song-time speed.
// One min/max query per visible column, never per PCM sample.
void DrawWaveform(const core::AudioWaveform& waveform,Rectangle bounds,
                  double from,double to,Color color) {
    if (waveform.Empty() || bounds.width<1 || bounds.height<2 || !(to>from)) return;
    const int columns=std::clamp(static_cast<int>(std::ceil(bounds.width)),1,2048);
    const float width=bounds.width/columns;
    const float center=bounds.y+bounds.height*0.5f;
    // Keep headroom even for brickwall-limited masters. The quiet outer
    // peak envelope preserves transients, while the brighter RMS interior
    // makes local energy changes visible even if every bin reaches full scale.
    const float radius=bounds.height*0.42f;
    Color peaks=color;
    peaks.a=static_cast<unsigned char>(color.a/3);
    for (int i=0;i<columns;++i) {
        const auto peak=waveform.Range(from+(to-from)*i/columns,
                                      from+(to-from)*(i+1)/columns);
        if (peak.maximum==0 && peak.minimum==0) continue;
        const float x=bounds.x+i*width;
        DrawRectangleRec({x,center-peak.maximum*radius,width,
            (peak.maximum-peak.minimum)*radius},peaks);
        const float rms=peak.Rms()*radius;
        if (rms>0)
            DrawRectangleRec({x,center-rms,width,2*rms},color);
    }
}

void DrawTimeline(Font font, const core::ChartEditor& draft,
                  const EditorUiState& ui, double previewBeat, bool nativeShell) {
    const auto strip = TimelineBounds(ui);
    if (!HasLayoutArea(strip)) return;
    const auto& loaded = draft.Data();
    const auto& chart = loaded.route;
    const double anchor = ui.draggingTimeline
        ? ui.dragViewBeat : ViewBeat(draft, previewBeat);
    const auto projection=MakeTimelineProjection(draft,ui,anchor);
    const double pixelsPerSecond=projection.pixelsPerSecond;
    if (!std::isfinite(pixelsPerSecond) || pixelsPerSecond<=0.0) return;
    const auto atX=[&](double songSeconds) {
        return projection.XAtSeconds(songSeconds);
    };
    const double leftTime=projection.SecondsAtX(strip.x+5.0f);
    const double rightTime=projection.SecondsAtX(strip.x+strip.width-5.0f);
    const double firstChartTime = core::TempoSecondsAtTick(
        chart, loaded.offsetSeconds, chart.firstBeat);
    const double lastChartTime = core::TempoSecondsAtTick(
        chart, loaded.offsetSeconds, chart.lengthBeats);
    const double visibleFrom = std::max(leftTime, firstChartTime);
    const double visibleTo = std::min(rightTime, lastChartTime);
    const float lineY = strip.y + 25.0f;

    DrawRectangleRounded(strip, UiRoundness(strip,8), 8,
                         Color{30, 37, 58, 255});
    DrawRectangleRoundedLinesEx(strip, UiRoundness(strip,8), 8, 1,
                                ApprovedUiTheme().divider);
    DrawRectangleRec({strip.x + 2.0f, strip.y + 2.0f,
                      PlayheadX(ui) - strip.x - 2.0f,
                      strip.height - 4.0f}, Color{41, 62, 70, 255});

    if (ui.waveformVisible && ui.waveform)
        DrawWaveform(*ui.waveform,{strip.x+5,strip.y+4,strip.width-10,strip.height-8},
                     leftTime,rightTime,Color{100,168,193,115});

    if (ui.tab == EditorTab::Route && ui.HasRange()) {
        const double halfStep = 0.5 * chart.ticksPerBeat / chart.snapDivision;
        const float firstX = atX(core::TempoSecondsAtTick(
            chart, loaded.offsetSeconds, ui.rangeFirst - halfStep));
        const float lastX = atX(core::TempoSecondsAtTick(
            chart, loaded.offsetSeconds, ui.rangeLast + halfStep));
        const float x1 = std::max(firstX, strip.x + 2.0f);
        const float x2 = std::min(lastX, strip.x + strip.width - 2.0f);
        if (x2 > x1)
            DrawRectangleRec({x1, strip.y + 2.0f, x2 - x1,
                              strip.height - 4.0f},
                             Color{187, 160, 231, 96});
    }
    DrawLineEx({strip.x + 4.0f, lineY},
               {strip.x + strip.width - 4.0f, lineY},
               2.0f, Color{98, 125, 150, 255});

    // Draw each tempo segment independently in SONG SECONDS. This avoids
    // scanning thousands of offscreen integer ticks or accumulating rounding
    // drift at BPM boundaries. The marker itself is local beat zero.
    // Authored maps are borrowed; only the empty-map fallback owns a
    // synthesized marker. Never copy the chart's tempo vector per frame.
    const auto fallback=chart.tempoMap.empty()
        ? core::EffectiveTempoMap(chart,loaded.offsetSeconds)
        : std::vector<core::TempoMarker>{};
    const auto& markers=chart.tempoMap.empty()?fallback:chart.tempoMap;
    if (visibleTo > visibleFrom) {
        const auto segments=TimelineTempoSegments(markers,visibleFrom,visibleTo);
        for (std::size_t i=segments.first;i<segments.second;++i) {
            const double origin = markers[i].timeMs / 1000.0;
            const double begin = std::max(visibleFrom,
                i == 0 ? visibleFrom : origin);
            const double finish = std::min(visibleTo,
                i+1 == markers.size() ? visibleTo
                                      : markers[i+1].timeMs / 1000.0);
            if (finish <= begin) continue;
            const double quarter = 60.0 / markers[i].bpm;
            const double subdivision = quarter / chart.snapDivision;
            const double subdivPx = subdivision * pixelsPerSecond;
            const double beatPx = quarter * pixelsPerSecond;
            const double barPx = 4.0 * beatPx;

            const auto lines = [&](double interval, int kind, long long stride) {
                if (!std::isfinite(interval) || interval <= 0.0 ||
                    stride < 1) return;
                const double low = std::ceil((begin-origin)/interval - 1e-8);
                const double high = std::floor((finish-origin)/interval + 1e-8);
                const long long from = static_cast<long long>(low);
                const long long through = static_cast<long long>(high);
                // At extremely high BPM, subpixel lines are aggregated
                // into only the visible resolution; always keep bar zero.
                const long long first = from >= 0
                    ? ((from + stride - 1) / stride) * stride
                    : -((-from) / stride) * stride;
                for (long long k = first; k <= through; k += stride) {
                    if (kind == 0 && k % chart.snapDivision == 0) continue;
                    if (kind == 1 && k % 4 == 0) continue;
                    const double songSeconds = origin + k * interval;
                    if (i + 1 < markers.size() &&
                        songSeconds >= markers[i+1].timeMs / 1000.0 - 1e-8)
                        break;
                    const float x = atX(songSeconds);
                    if (x < strip.x + 5.0f ||
                        x > strip.x + strip.width - 5.0f)
                        continue;
                    const bool bar = kind == 2;
                    const bool major = kind == 1;
                    DrawLineEx(
                        {x, strip.y + (bar ? 11.0f
                                    : major ? 17.0f : 23.0f)},
                        {x, strip.y + 34.0f},
                        bar ? 2.5f : major ? 1.5f : 1.0f,
                        bar ? Color{199, 220, 234, 255}
                            : major ? Color{113, 141, 163, 255}
                                    : Color{81, 126, 135, 220});
                }
            };
            if (subdivPx >= 4.0)
                lines(subdivision, 0, 1);
            if (beatPx >= 3.0)
                lines(quarter, 1, 1);
            if (barPx > 0.0) {
                const long long stride = std::max(1LL, static_cast<long long>(
                    std::ceil(3.0 / barPx)));
                lines(4.0 * quarter, 2, stride);
            }
        }

        // BPM markers are always highlighted even when nearby subdivisions
        // become too dense to display at the current zoom.
        const auto firstMarker=std::lower_bound(markers.begin()+1,markers.end(),visibleFrom,
            [](const core::TempoMarker& marker,double seconds) {
                return marker.timeMs/1000.0<seconds;
            });
        for(auto it=firstMarker;it!=markers.end();++it) {
            const double time=it->timeMs/1000.0;
            if(time>visibleTo) break;
            const float x=atX(time);
            DrawLineEx({x, strip.y + 7.0f},
                       {x, strip.y + 39.0f},
                       3.0f, Color{111, 233, 198, 255});
            const char* bpm=TextFormat("%.1f",it->bpm);
            Label(font, bpm, x + 5.0f, strip.y + 1.0f, 10.0f,
                  Color{160, 236, 211, 255});
        }
    }

    // Notes are indexed by absolute time, not cached editor tick positions.
    // Tempo changes may shift their tick IDs without moving a note in audio.
    const auto firstVisible = std::lower_bound(
        chart.turns.begin(), chart.turns.end(), leftTime * 1000.0,
        [&](const core::TurnEvent& event, double ms) {
            return core::EventTimeMs(chart, event, loaded.offsetSeconds) < ms;
        });
    const auto* selected=draft.SelectedTurnTimeMs()?draft.SelectedTurn():nullptr;
    for (auto it = firstVisible; it != chart.turns.end(); ++it) {
        const auto& event = *it;
        const double songSeconds =
            core::EventTimeMs(chart, event, loaded.offsetSeconds) / 1000.0;
        if (songSeconds > rightTime) break;
        const float x = atX(songSeconds);
        if (x < strip.x + 8.0f ||
            x > strip.x + strip.width - 8.0f)
            continue;
        const auto next = std::upper_bound(markers.begin(), markers.end(),
            songSeconds * 1000.0,
            [](double ms, const core::TempoMarker& marker) {
                return ms < marker.timeMs;
            });
        const double bpm = (next == markers.begin() ? markers.front()
                                                   : *(next - 1)).bpm;
        const float spacing = static_cast<float>(
            pixelsPerSecond * 60.0 / (bpm * chart.snapDivision));
        const bool narrow = spacing < 26.0f;
        const float width = narrow
            ? std::max(5.0f, std::min(spacing - 1.0f, 12.0f))
            : std::min(spacing - 3.0f, 32.0f);
        const Color tint = core::IsDiagonal(event.direction)
            ? Color{175, 155, 236, 255} : Color{111, 214, 202, 255};
        DrawRectangleRec({x - width * 0.5f, strip.y + 29.0f,
                          width, 12.0f}, tint);
        if (selected==&event) {
            // Exact event selection is visible even if another authored
            // note shares this musical grid cell.
            DrawRectangleLinesEx({x - width * 0.5f - 2.0f,
                                  strip.y + 27.0f,
                                  width + 4.0f, 16.0f},
                                 2.0f, ApprovedUiTheme().foreground);
        }
        if (!narrow) {
            constexpr const char* names[] = {
                "U", "R", "D", "L", "UR", "DR", "DL", "UL"
            };
            const char* label = names[static_cast<int>(event.direction)];
            const float widthText = MeasureTextEx(font, label, 10, 1).x;
            Label(font, label, x - widthText * 0.5f,
                  strip.y + 29.0f, 10, Color{18, 26, 36, 255});
        }
    }
    if (ui.pendingExactPlacementTimeMs) {
        const float x = atX(*ui.pendingExactPlacementTimeMs / 1000.0);
        if (x >= strip.x + 6.0f &&
            x <= strip.x + strip.width - 6.0f) {
            DrawLineEx({x, strip.y + 3.0f},
                       {x, strip.y + 43.0f},
                       2.0f, Color{255, 190, 112, 255});
        }
    }
    DrawRectangleRec({PlayheadX(ui) - 3.0f, strip.y + 1.0f,
                      6.0f, strip.height - 2.0f},
                     ApprovedUiTheme().softBlue);
    DrawCircleV({PlayheadX(ui), lineY}, 5.0f, ApprovedUiTheme().foreground);

    if (ui.tab == EditorTab::Route && !nativeShell) {
        const auto& colors = ApprovedUiTheme();
        Label(font, TextFormat("SNAP 1/%d", chart.snapDivision),
              730, strip.y + 4, 12, colors.softBlue);
        Label(font, TextFormat("ZOOM %d%%",
              static_cast<int>(std::round(PixelsPerBeat(ui)/36.0f*100))),
              730, strip.y + 29, 12, colors.secondaryText);
        CompactButton(font, kSnapLess, "-", colors.rose);
        CompactButton(font, kSnapMore, "+", colors.softBlue);
        CompactButton(font, kZoomOut, "-", colors.rose);
        CompactButton(font, kZoomIn, "+", colors.softBlue);
    }
}

void DrawSongOverview(Font font, const core::ChartEditor& draft,
                      const EditorSetView& set, const EditorUiState& ui, double songSeconds) {
    const Rectangle bounds = OverviewBounds(ui);
    if (!HasLayoutArea(bounds)) return;
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
    const float cursor = SongOverviewX(songSeconds, duration, bounds);
    const float lineY = bounds.y + bounds.height - 8.0f;
    // Song Setup describes the shared audio, not the currently loaded
    // difficulty. Do not even calculate its strain graph in that view.
    if (ui.tab==EditorTab::Route && bounds.height>24.0f) {
        const auto& graph=draft.DifficultyGraph();
        if (graph.maximum>0.0 && !graph.peaks.empty()) {
            const float top=bounds.y+14.0f;
            const float width=bounds.width/graph.peaks.size();
            if (width>0) for (std::size_t i=0;i<graph.peaks.size();++i) {
                const float height=static_cast<float>(graph.peaks[i]/graph.maximum)*(lineY-top);
                if (height>0) DrawRectangleRec({bounds.x+i*width,lineY-height,width,height},
                                               Color{181,164,243,150});
            }
        }
    }
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
    const auto& chart=draft.Data().route;
    const auto fallback=(!set.managed || set.tempoMap.empty()) && chart.tempoMap.empty()
        ? core::EffectiveTempoMap(chart,draft.Data().offsetSeconds)
        : std::vector<core::TempoMarker>{};
    const auto& tempoMarkers=set.managed && !set.tempoMap.empty()?set.tempoMap:
        (chart.tempoMap.empty()?fallback:chart.tempoMap);
    const int total = static_cast<int>(std::ceil(duration));
    const std::string end=TextFormat("%d:%02d", total / 60, total % 60);
    const float timeWidth=MeasureTextEx(font,end.c_str(),11.0f,1.0f).x;
    const float timeX=right-timeWidth-7.0f;
    float previousTextRight = bounds.x;
    for (const auto& marker : tempoMarkers) {
        const float x = SongOverviewX(marker.timeMs / 1000.0, duration, bounds);
        DrawLineEx({x, bounds.y + 5.0f}, {x, lineY+4.0f},
                   2.0f, Color{128, 223, 210, 255});
        const char* name = TextFormat("%g BPM", marker.bpm);
        const float width = MeasureTextEx(font, name, 11.0f, 1.0f).x;
        const float labelX = std::clamp(x + 4.0f, bounds.x + 7.0f,
            std::max(bounds.x + 7.0f, right - width - 6.0f));
        if (labelX > previousTextRight + 6.0f && labelX+width+6.0f<timeX) {
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
    Label(font, end.c_str(), timeX, bounds.y + 3.0f, 11.0f,
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
                       double previewSeconds,bool nativeShell,
                       PathRibbonWorkspace* ribbon) {
    const auto& chart = draft.Data().route;
    const auto& theme = ApprovedUiTheme();
    const bool running = previewPlaying && !previewPaused;
    const bool routeTab = ui.tab == EditorTab::Route;
    const Color violet = theme.rose;
    const Color sky = theme.softBlue;
    const Color muted = theme.secondaryText;

    if (routeTab) {
        // Unlike the old horizontal button bars, this six-icon vertical
        // rail never reduces the usable height of the playing field.
        if (!nativeShell) {
        EditorIconButton(kMirrorHorizontal,EditorIcon::FlipH,violet);
        EditorIconButton(kMirrorVertical,EditorIcon::FlipV,violet);
        EditorIconButton(kRotateCounterclockwise,EditorIcon::RotateL,sky);
        EditorIconButton(kRotateClockwise,EditorIcon::RotateR,sky);
        }
        if (!nativeShell)
            EditorIconButton(CompareToggleBounds(ui),EditorIcon::Compare,
                             set.managed?sky:muted,ui.comparisonPickerOpen);
        if (!nativeShell) {
        EditorIconButton(kRouteTest,EditorIcon::Test,sky);
        }

        const Rectangle previewArea = PreviewBounds(ui);
        EditorComparisonFrame fallback;
        core::TimedRoute one;
        if (!comparisons && (HasLayoutArea(previewArea) || !nativeShell)) {
            if (one.Compile(draft.Data().route,draft.Data().offsetSeconds))
                fallback.panes[0]={&one,draft.Data().title,true};
            fallback.count=1;
            fallback.directionPressCounts=draft.DirectionPressCounts();
            comparisons=&fallback;
        }
        if (comparisons && HasLayoutArea(previewArea)) {
        DrawRectangleRounded(previewArea,UiRoundness(previewArea,12),10,
                             Color{25,33,53,235});
        DrawRectangleRoundedLinesEx(previewArea,UiRoundness(previewArea,12),
                                    10,1,theme.divider);
        Skin editorSkin=skin;
        editorSkin.lookaheadMs=settings.editorPreviewLengthMs;
        PathRibbonWorkspace standaloneRibbon;
        DrawComparisonPreviews(font,ui,*comparisons,previewSeconds,
            settings.editorPreviewSpeed,editorSkin,ribbon?*ribbon:standaloneRibbon);
        const int focused=std::clamp(ui.comparisonFocusedSlot,0,
                                    std::max(0, comparisons->count-1));
        const Rectangle focusedBounds=ComparisonPreviewBounds(
            ui,comparisons->count,focused);
        Label(font,TextFormat("%.1f",draft.Difficulty().overall),
              focusedBounds.x+focusedBounds.width-43,
              focusedBounds.y+23,12,sky);
        }

        if (!nativeShell) {
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
        }
    } else if (!nativeShell && (ui.tab == EditorTab::Set || ui.tab == EditorTab::Audio)) {
        const auto panel = [&](Rectangle bounds) {
            DrawRectangleRounded(bounds,UiRoundness(bounds,12),9,theme.surface);
            DrawRectangleRoundedLinesEx(bounds,UiRoundness(bounds,12),9,1,theme.divider);
        };
        const auto textBox = [&](Rectangle bounds,const char* label,
                                 const std::string& value,bool active) {
            Label(font,label,bounds.x,bounds.y-15,11,muted);
            DrawRectangleRounded(bounds,UiRoundness(bounds,7),8,
                active?Color{62,59,92,255}:Color{31,41,63,255});
            DrawRectangleRoundedLinesEx(bounds,UiRoundness(bounds,7),8,
                active?2.0f:1.0f,active?sky:theme.divider);
            Label(font,UiFitText(font,value,bounds.width-14,13).c_str(),
                bounds.x+7,bounds.y+8,13,theme.foreground);
        };
        panel(kAudioTempoPanel);
        Label(font,"AUDIO & TIMING",361,126,15,violet);
        DrawUiThemedButton(font,kSong,hasAudio?"AUDIO LOADED / CHANGE":"ATTACH AUDIO",
            UiButtonVariant::Quiet,{}, {},theme,12);
        const std::string audioLabel=set.audioName.empty()?songName:set.audioName;
        Label(font,UiFitText(font,audioLabel,268,11).c_str(),361,184,11,muted);
        textBox(kAudioPreviewMs,"MENU PREVIEW START / MS",
            ui.editingSetPreview?ui.setPreviewBuffer:
            set.managed?std::to_string(set.previewStartMs):"CREATE SONG SET FIRST",
            ui.editingSetPreview);
        Label(font,"TEMPO MAP",361,246,13,violet);
        Label(font,"OFFSET / MS",361,266,10,muted);
        Label(font,"BPM",471,266,10,muted);
        const auto changes=set.managed&&!set.tempoMap.empty()
            ?set.tempoMap:core::EffectiveTempoMap(chart,draft.Data().offsetSeconds);
        const int count=static_cast<int>(changes.size());
        const int first=std::clamp(ui.tempoFirstVisible,0,std::max(0,count-5));
        for (int row=0;row<5;++row) {
            const int index=first+row;
            if(index>=count)break;
            const auto& marker=changes[static_cast<std::size_t>(index)];
            const auto cell=[&](Rectangle bounds,const std::string& value,bool editing) {
                DrawRectangleRounded(bounds,UiRoundness(bounds,7),8,
                    editing?Color{62,59,92,255}:Color{31,41,63,255});
                DrawRectangleRoundedLinesEx(bounds,UiRoundness(bounds,7),8,
                    editing?2.0f:1.0f,editing?sky:theme.divider);
                Label(font,UiFitText(font,value,bounds.width-12,12).c_str(),
                    bounds.x+6,bounds.y+6,12,theme.foreground);
            };
            char bpm[48]{};
            std::snprintf(bpm,sizeof(bpm),"%g",marker.bpm);
            const bool msActive=ui.tempoEditingRow==index&&!ui.tempoEditingBpm;
            const bool bpmActive=ui.tempoEditingRow==index&&ui.tempoEditingBpm;
            cell(TempoOffsetCell(row),msActive?ui.tempoBuffer:std::to_string(marker.timeMs),msActive);
            cell(TempoBpmCell(row),bpmActive?ui.tempoBuffer:std::string(bpm),bpmActive);
            if(index>0)CompactButton(font,TempoDeleteCell(row),"DEL",violet);
            else Label(font,"START",TempoDeleteCell(row).x+8,TempoDeleteCell(row).y+7,10,sky);
        }
        if (count>5) Label(font,TextFormat("%d-%d / %d",first+1,std::min(first+5,count),count),361,420,10,muted);
        DrawUiThemedButton(font,kTempoAdd,"+ ADD BPM MARKER",UiButtonVariant::Cool,{}, {},theme,12);
        if (!nativeShell) {
            panel(kSetupMetadataPanel);
            panel(kSetupDifficultyPanel);
            Label(font,"SONG DETAILS",54,126,15,violet);
            Label(font,"DIFFICULTIES",658,126,15,sky);
            std::string tags;
            for(const auto& tag:set.tags) { if(!tags.empty())tags+=", "; tags+=tag; }
            const auto field=[&](EditorSetField kind,Rectangle bounds,
                                 const char* label,const std::string& value) {
                const bool focused=ui.editingSetField==kind;
                textBox(bounds,label,focused?ui.setFieldBuffer:value,focused);
            };
            field(EditorSetField::Title,kSetTitleField,"SONG TITLE",set.title);
            field(EditorSetField::Artist,kSetArtistField,"ARTIST",set.artist);
            field(EditorSetField::Mapper,kSetMapperField,"MAPPER",set.mapper);
            field(EditorSetField::Tags,kSetTagsField,"TAGS",tags);
            CompactButton(font,kSetBackground,set.hasBackground?"BACKGROUND":"ADD BG",sky);
            CompactButton(font,kSetRemoveBackground,"REMOVE BG",violet);
            CompactButton(font,kSetImport,"IMPORT",sky);
            CompactButton(font,kSetExport,"EXPORT",violet);
            CompactButton(font,kSetOpenFolder,"OPEN FOLDER",sky);
            CompactButton(font,kSetRestore,"RESTORE",violet);
            if(!set.managed) Label(font,"CREATE SONG SET TO EDIT DETAILS",54,449,10,muted);
            const auto selectedName = set.selected >= 0
                ? set.difficulties[static_cast<std::size_t>(set.selected)] : draft.Data().title;
            textBox(kTitle,"DIFFICULTY NAME",
                ui.active==EditorField::Title?ui.buffer:selectedName,
                ui.active==EditorField::Title);
            if (set.newDifficultyDraft)
                DrawUiThemedButton(font,kSetupEnter,(draft.Data().title + " (DRAFT)").c_str(),
                    UiButtonVariant::Quiet,{}, {},theme,12);
            CompactButton(font,kSetNew,"NEW",sky);
            CompactButton(font,kSetDuplicate,"DUPLICATE",violet);
            for(int row=0;row<5;++row) {
                const int index=set.firstVisible+row;
                if(index>=static_cast<int>(set.difficulties.size()))break;
                const auto bounds=SetDifficultyRow(row);
                const bool active=index==set.current;
                DrawRectangleRounded(bounds,UiRoundness(bounds,7),8,
                    index==set.selected?Color{62,61,95,255}:Color{31,41,63,255});
                DrawRectangleRoundedLinesEx(bounds,UiRoundness(bounds,7),8,
                    active?2.0f:1.0f,active?sky:theme.divider);
                Label(font,UiFitText(font,set.difficulties[static_cast<std::size_t>(index)],
                    bounds.width-16,13).c_str(),bounds.x+8,bounds.y+8,13,theme.foreground);
            }
            if(set.difficulties.empty())Label(font,"NO SAVED DIFFICULTIES",658,275,11,muted);
            CompactButton(font,kSetDeleteDifficulty,"DELETE CHART",theme.danger);
            CompactButton(font,kSetDeleteSong,"DELETE SET",theme.danger);
        }
    } else if (!nativeShell) {
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

    if(routeTab) DrawTimeline(font,draft,ui,previewBeat,nativeShell);
    DrawSongOverview(font,draft,set,ui,previewSeconds);

    // Timeline and song overview still use raylib; transport chrome is RmlUi.
    const char* speeds[5]={"100%","75%","50%","25%","10%"};
    int rate=0;
    for(int i=0;i<5;++i)
        if(std::abs(playbackSpeed-kSpeeds[i])<.001)rate=i;
    if (!nativeShell) {
    EditorIconButton(kEditorPreviewPlay,
        running?EditorIcon::Pause:EditorIcon::Play,sky);
    EditorIconButton(kEditorPreviewRestart,EditorIcon::Stop,violet);

    DrawUiThemedButton(font,kEditorRateDropdown,
        TextFormat("%.0f%%",playbackSpeed*100.0),
        UiButtonVariant::Quiet,{}, {},theme,13);
    DrawUiThemedButton(font,kRouteTimeValue,
        TextFormat("%lld ms",RouteTimeMs(draft,previewBeat)),
        UiButtonVariant::Quiet,{}, {},theme,12);
    }

    if(routeTab) {
        if (!nativeShell) DrawComparisonPicker(font,ui,set);
        // Only fallback controls use the legacy hitboxes. Native controls
        // own their own geometry and tooltip metadata.
        if (!nativeShell) {
        EditorToolTip(font,ui,kMirrorHorizontal,
            "Flip horizontally  /  "+
            KeyboardChordName(settings.Shortcut(ShortcutAction::EditorMirrorH)));
        EditorToolTip(font,ui,kMirrorVertical,
            "Flip vertically  /  "+
            KeyboardChordName(settings.Shortcut(ShortcutAction::EditorMirrorV)));
        EditorToolTip(font,ui,kRotateCounterclockwise,"Rotate 90 left");
        EditorToolTip(font,ui,kRotateClockwise,"Rotate 90 right");
        EditorToolTip(font,ui,kRouteTest,
            "Test chart  /  "+
            KeyboardChordName(settings.Shortcut(ShortcutAction::EditorTest)));
        }
        if (!nativeShell) EditorToolTip(font,ui,CompareToggleBounds(ui),"Compare charts");
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
