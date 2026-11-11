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

    const auto ms = static_cast<long long>(std::llround(songSeconds * 1000.0));
    const auto absolute = std::llabs(ms);
    const std::string current = TextFormat("TIME %s%lld:%02lld.%03lld",
        ms < 0 ? "-" : "", absolute / 60000,
        (absolute / 1000) % 60, absolute % 1000);
    Label(font, current.c_str(),
          kRouteTimeValue.x, kRouteTimeValue.y + 5, 13.0f,
          Color{158, 225, 212, 255});
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
        586, 424, 12.0f, Color{183, 202, 216, 255});
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

    DrawRectangleRounded(strip, 0.15f, 5, Color{34, 44, 59, 255});
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
                     Color{241, 206, 118, 255});
    DrawCircleV({kPlayheadX, lineY}, 5.0f, Color{253, 246, 215, 255});

    // These controls affect the authored ruler only; their hitboxes stay
    // outside both timelines. SET does not mutate grid/snap settings.
    if (ui.tab == EditorTab::Route || ui.tab == EditorTab::Field) {
        CompactButton(font, kSnapLess, "SNAP -", Color{128, 221, 202, 255});
        CompactButton(font, kSnapMore, "SNAP +", Color{128, 221, 202, 255});
        CompactButton(font, kZoomOut, "ZOOM -", Color{125, 167, 205, 255});
        CompactButton(font, kZoomIn, "ZOOM +", Color{125, 167, 205, 255});
        Label(font, TextFormat("1/%d", chart.snapDivision),
              kTimelineX + 9.0f, kTimelineY + 3.0f, 11.0f,
              Color{164, 188, 208, 255});
    }
}

void DrawSongOverview(Font font, const core::ChartEditor& draft,
                      const EditorSetView& set, double songSeconds) {
    const Rectangle bounds = kSongOverview;
    DrawRectangleRounded(bounds, 0.14f, 5, Color{34, 44, 59, 255});
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
                         Color{45, 94, 97, 255});
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
    DrawCircleV({cursor, lineY}, 4.0f, Color{255, 244, 209, 255});
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
                       double previewSeconds) {
    const auto& chart = draft.Data().route;
    Label(font, "CHART EDITOR", 49, 80, 22, Color{194, 236, 231, 255});
    const bool routeTab = ui.tab == EditorTab::Route;
    const auto navButton = routeTab ? CompactButton : Button;
    navButton(font, kTabSet, "SET", Color{244, 197, 124, 255},
              ui.tab == EditorTab::Set);
    navButton(font, kTabRoute, "ROUTE", Color{126, 219, 209, 255},
              routeTab);
    navButton(font, kTabAudio, "AUDIO", Color{131, 190, 237, 255},
              ui.tab == EditorTab::Audio);
    navButton(font, kTabField, "FIELD", Color{239, 203, 129, 255},
              ui.tab == EditorTab::Field);
    navButton(font, kEditorBack, "BACK", Color{247, 202, 145, 255}, false);

    const bool running = previewPlaying && !previewPaused;
    if (routeTab) {
        CompactButton(font, kRouteTest, "TEST CHART",
                      Color{188, 176, 232, 255});
    } else {
        // The SET/AUDIO/FIELD transport stays operable but no longer
        // advertises obsolete hardcoded key names under every button.
        Button(font, kSong, "SONG", Color{97, 222, 193, 255});
        Button(font, kPlay, running ? "PAUSE" : "PLAY",
               Color{137, 211, 235, 255});
        Button(font, kStop, "RESTART", Color{211, 178, 137, 255});
        Button(font, kTest, "TEST", Color{188, 176, 232, 255});
        Label(font, hasAudio ? Truncate(songName, 26).c_str() : "NO AUDIO",
              544, 513, 12,
              hasAudio ? Color{178, 227, 222, 255}
                       : Color{242, 181, 164, 255});
    }

    if (ui.tab == EditorTab::Route) {
        CompactButton(font, kCopyTurns, "COPY", Color{126, 190, 221, 255});
        CompactButton(font, kPasteTurns, "PASTE", Color{127, 224, 199, 255});
        CompactButton(font, kCutTurns, "CUT", Color{216, 166, 155, 255});
        CompactButton(font, kUndoEdit, "UNDO", Color{169, 188, 217, 255});
        CompactButton(font, kRedoEdit, "REDO", Color{169, 188, 217, 255});
        CompactButton(font, kMirrorHorizontal, "FLIP H",
                      Color{190, 168, 229, 255});
        CompactButton(font, kMirrorVertical, "FLIP V",
                      Color{190, 168, 229, 255});
        CompactButton(font, kRotateCounterclockwise, "ROT -90",
                      Color{140, 196, 237, 255});
        CompactButton(font, kRotateClockwise, "ROT +90",
                      Color{140, 196, 237, 255});
        CompactButton(font, kCompareToggle,
                      ui.comparisonFiles.empty() ? "COMPARE +" : "COMPARE",
                      set.managed ? Color{124, 217, 206, 255}
                                  : Color{112, 131, 147, 255},
                      ui.comparisonPickerOpen);
        EditorComparisonFrame fallback;
        core::TimedRoute one;
        if (!comparisons) {
            if (one.Compile(draft.Data().route, draft.Data().offsetSeconds))
                fallback.panes[0] = {&one, draft.Data().title, true};
            fallback.count = 1;
            fallback.directionPressCounts = DirectionPressCounts(chart);
            comparisons = &fallback;
        }
        // ROUTE presentation is intentionally independent of PLAY's SKIN.
        // Reuse the same trajectory styling, overriding only the editor's
        // chosen speed and lookahead duration. Practice slowdown still uses
        // one shared song-time clock for every comparison pane.
        Skin editorSkin = skin;
        editorSkin.lookaheadMs = settings.editorPreviewLengthMs;
        DrawComparisonPreviews(font, *comparisons, previewSeconds,
            settings.editorPreviewSpeed /
                static_cast<float>(std::max(0.1, playbackSpeed)), editorSkin);
        Label(font, TextFormat("SPD %.0f", settings.editorPreviewSpeed),
              745, 403, 12, Color{174, 222, 226, 255});
        CompactButton(font, kEditorSpeedMinus, "-", Color{134, 176, 212, 255});
        CompactButton(font, kEditorSpeedPlus, "+", Color{134, 212, 180, 255});
        Label(font, TextFormat("PATH %.0f", settings.editorPreviewLengthMs),
              745, 437, 12, Color{174, 222, 226, 255});
        CompactButton(font, kEditorPathMinus, "-", Color{134, 176, 212, 255});
        CompactButton(font, kEditorPathPlus, "+", Color{134, 212, 180, 255});
        // Place the rating within the focused pane, not beneath the newly
        // enlarged previews where it would overlap the route drawing.
        const int focused = std::clamp(ui.comparisonFocusedSlot, 0,
                                       comparisons->count - 1);
        const Rectangle focusedBounds = ComparisonPreviewBounds(
            comparisons->count, focused);
        Label(font, TextFormat("DIFF %.1f", draft.Difficulty().overall),
              focusedBounds.x + focusedBounds.width - 82,
              focusedBounds.y + 6, 11, Color{239, 202, 137, 255});
        // Starting direction is owned by FIELD, not duplicated here.
        const auto* turn = draft.SelectedTurn();
        const auto incoming = core::IncomingHeadingAt(chart, draft.SelectedBeat());
        const auto outgoing = turn ? turn->direction : incoming;
        Label(font, TextFormat("%.3f  %s",
                              static_cast<double>(draft.SelectedBeat()) /
                                  chart.ticksPerBeat,
                              turn ? core::DirectionName(turn->direction) : "STRAIGHT"),
              744, 244, 12, Color{216, 232, 239, 255});
        // Full-chart totals, including each diagonal's two cardinals.
        const auto& pressCounts = comparisons->directionPressCounts;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col) {
                const auto heading = DirectionPadHeading(row, col);
                const std::string label = heading
                    ? std::to_string(pressCounts[static_cast<std::size_t>(*heading)])
                    : "DEL";
                DirectionButton(font, DirectionPadBounds(row, col),
                                label.c_str(),
                                !heading ? Color{127, 140, 156, 255}
                                  : core::IsDiagonal(*heading)
                                    ? Color{186, 160, 235, 255}
                                    : Color{110, 220, 198, 255},
                                heading ? (turn && *heading == outgoing)
                                        : false); // DEL is always neutral, never selected
            }
    } else if (ui.tab == EditorTab::Audio) {
        DrawTextField(font, kTitle, "DIFFICULTY", draft.Data().title,
                      EditorField::Title, ui);
        // Managed sets use the manifest's tempo, not the currently open
        // chart's cached BPM metadata. A draft uses its own tempo map.
        const auto changes = set.managed && !set.tempoMap.empty()
            ? set.tempoMap
            : core::EffectiveTempoMap(chart, draft.Data().offsetSeconds);
        const auto& rating = draft.Difficulty();
        Label(font, TextFormat("DIFF %.1f", rating.overall),
              92, 252, 12, Color{239, 202, 137, 255});
        Label(font, "SONG PREVIEW (MS)", kAudioPreviewMs.x,
              kAudioPreviewMs.y - 22.0f, 15,
              Color{172, 199, 218, 255});
        DrawRectangleRounded(kAudioPreviewMs, 0.14f, 5,
                             ui.editingSetPreview ? Color{47, 68, 78, 255}
                                                  : Color{35, 43, 56, 255});
        DrawRectangleRoundedLinesEx(kAudioPreviewMs, 0.14f, 5,
                                    ui.editingSetPreview ? 2.0f : 1.0f,
                                    ui.editingSetPreview ? Color{129, 235, 214, 255}
                                                         : Color{91, 114, 139, 255});
        const std::string preview = ui.editingSetPreview
            ? ui.setPreviewBuffer
            : (set.managed ? std::to_string(set.previewStartMs)
                           : "SAVE SONG FIRST");
        Label(font, preview.c_str(),
              kAudioPreviewMs.x + 10.0f, kAudioPreviewMs.y + 9.0f, 16,
              Color{225, 235, 241, 255});

        Label(font, "OFFSET (MS)", 104, 267, 13,
              Color{184, 215, 231, 255});
        Label(font, "BPM", 350, 267, 13, Color{184, 215, 231, 255});
        const int count = static_cast<int>(changes.size());
        const int first = std::clamp(ui.tempoFirstVisible, 0,
                                     std::max(0, count - 5));
        for (int row = 0; row < 5; ++row) {
            const int index = first + row;
            if (index >= count) break;
            const auto& marker = changes[static_cast<std::size_t>(index)];
            const auto field = [&](Rectangle rect, const std::string& value,
                                   bool active) {
                DrawRectangleRounded(rect, 0.12f, 5,
                    active ? Color{47, 73, 77, 255}
                           : Color{35, 43, 56, 255});
                DrawRectangleRoundedLinesEx(rect, 0.12f, 5,
                    active ? 1.8f : 1.0f,
                    active ? Color{118, 226, 206, 255}
                           : Color{95, 123, 151, 255});
                Label(font, value.c_str(), rect.x + 9, rect.y + 6, 14,
                      Color{222, 237, 237, 255});
            };
            char bpm[48]{};
            std::snprintf(bpm, sizeof(bpm), "%g", marker.bpm);
            const bool editingMs = ui.tempoEditingRow == index &&
                                   !ui.tempoEditingBpm;
            const bool editingBpm = ui.tempoEditingRow == index &&
                                    ui.tempoEditingBpm;
            field(TempoOffsetCell(row),
                editingMs ? ui.tempoBuffer : std::to_string(marker.timeMs),
                editingMs);
            field(TempoBpmCell(row),
                editingBpm ? ui.tempoBuffer : std::string(bpm),
                editingBpm);
            if (index > 0) {
                CompactButton(font, TempoDeleteCell(row), "DEL",
                              Color{221, 160, 159, 255});
            } else {
                Label(font, "START", TempoDeleteCell(row).x + 7,
                      TempoDeleteCell(row).y + 8, 11,
                      Color{157, 190, 210, 255});
            }
        }
        CompactButton(font, kTempoPrev, "PREV",
                      Color{130, 169, 207, 255});
        CompactButton(font, kTempoNext, "NEXT",
                      Color{130, 169, 207, 255});
        CompactButton(font, kTempoAdd, "ADD BPM MARKER",
                      Color{119, 223, 194, 255});
        Label(font, TextFormat("%d / %d", count ? first + 1 : 0, count),
              695, 318, 12, Color{177, 203, 221, 255});
    } else if (ui.tab == EditorTab::Set) {
        const Color heading{186, 219, 231, 255};
        const Color ordinary{211, 226, 236, 255};
        const Color amber{239, 202, 137, 255};
        Label(font, "SONG SET", 52, 170, 20, heading);
        if (set.managed) {
            Label(font, Truncate(set.title, 35).c_str(), 52, 201, 20, ordinary);
            if (!set.artist.empty())
                Label(font, Truncate(set.artist, 44).c_str(),
                      52, 233, 14, heading);
            Label(font, Truncate(set.audioName, 44).c_str(),
                  52, 255, 14, heading);
            if (set.newDifficultyDraft)
                Label(font, "NEW DIFFICULTY / UNSAVED", 505, 187, 12, amber);
        } else {
            Label(font, "NOT INSTALLED YET", 52, 208, 20, amber);
            Label(font, "ATTACH AUDIO, THEN CTRL+S", 52, 242, 15, heading);
            Label(font, "TO CREATE A SONG SET", 52, 265, 15, heading);
        }
        if (set.managed) {
            Button(font, kSetOpenFolder, "OPEN FOLDER", Color{174, 198, 234, 255});
            Button(font, kSetBackground, set.hasBackground ? "REPLACE BG" : "ADD BG",
                   Color{126, 219, 209, 255});
            Button(font, kSetRemoveBackground, "REMOVE BG", Color{227, 198, 137, 255});
        }
        Button(font, kSetNew, "NEW", Color{239, 203, 129, 255});
        Button(font, kSetDuplicate, "COPY", Color{162, 178, 239, 255});
        Button(font, kSetImport, "IMPORT", Color{122, 209, 226, 255});
        Button(font, kSetExport, "EXPORT", Color{122, 209, 226, 255});
        Button(font, kSetDeleteDifficulty, "DELETE", Color{235, 150, 146, 255});
        Button(font, kSetDeleteSong, "DELETE SET", Color{235, 150, 146, 255});
        Button(font, kSetRestore, "RESTORE LAST", Color{227, 198, 137, 255});
        Label(font, "DIFFICULTIES", 503, 170, 19, heading);
        for (int row = 0; row < 5; ++row) {
            const int index = set.firstVisible + row;
            if (index >= static_cast<int>(set.difficulties.size())) break;
            const Rectangle rect = SetDifficultyRow(row);
            const bool selected = index == set.selected;
            const bool active = index == set.current;
            DrawRectangleRounded(rect, 0.12f, 5,
                                 selected ? Color{55, 78, 81, 255}
                                          : Color{35, 43, 56, 255});
            DrawRectangleRoundedLinesEx(rect, 0.12f, 5, 1.3f,
                                        active ? Color{239, 203, 129, 255}
                                               : Color{101, 134, 159, 255});
            Label(font, Truncate(set.difficulties[index], 27).c_str(),
                  rect.x + 12, rect.y + 6, 16, ordinary);
            if (active)
                Label(font, "EDITING", rect.x + 311, rect.y + 9, 12, amber);
        }
        // The list scrolls with the mouse wheel; clicking any row opens it.
        // No paging buttons or duplicate OPEN action.
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

    DrawTimeline(font, draft, ui, previewBeat);
    DrawSongOverview(font, draft, set, previewSeconds);
    if (routeTab) {
        // Practice speed now lives below the ruler, not beside TEST.
        const char* speeds[5] = {"100%", "75%", "50%", "25%", "10%"};
        for (int i = 0; i < 5; ++i)
            CompactButton(font, RouteSpeedBounds(i), speeds[i],
                Color{125, 183, 222, 255},
                std::abs(playbackSpeed - kSpeeds[i]) < 0.001);
        DrawComparisonPicker(font, ui, set);
    }
}

} // namespace locus::app
