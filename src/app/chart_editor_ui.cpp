#include "app/chart_editor_ui.h"
#include "app/chart_editor_layout.h"
#include "core/judgement.h"
#include "app/route_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace locus::app {
using namespace detail;
namespace {

void Label(Font font, const char* value, float x, float y, float size, Color color) {
    DrawTextEx(font, value, {x, y}, size, 1.0f, color);
}

std::string Truncate(const std::string& value, std::size_t maxBytes) {
    if (value.size() <= maxBytes || maxBytes <= 3) return value;
    const std::size_t limit = maxBytes - 3;
    std::size_t position = 0;
    while (position < value.size()) {
        const unsigned char first = static_cast<unsigned char>(value[position]);
        const std::size_t characterBytes = first < 0x80 ? 1
                                           : first < 0xe0 ? 2
                                           : first < 0xf0 ? 3 : 4;
        if (position + characterBytes > limit) break;
        position += characterBytes;
    }
    return value.substr(0, position) + "...";
}

void Button(Font font, Rectangle rect, const char* text, Color tint, bool active = false) {
    DrawRectangleRounded(rect, 0.16f, 5,
                         active ? Color{45, 81, 88, 255}
                                : Color{36, 43, 58, 255});
    DrawRectangleRoundedLinesEx(rect, 0.16f, 5, 1.7f, tint);
    const Vector2 measure = MeasureTextEx(font, text, 17.0f, 1.0f);
    Label(font, text, rect.x + (rect.width - measure.x) / 2.0f,
          rect.y + (rect.height - measure.y) / 2.0f,
          17.0f, Color{232, 238, 245, 255});
}

// The nine compass pads remain clickable, with a small key-label line.
void DirectionButton(Font font, Rectangle rect, const char* name,
                     const char* keys, Color tint, bool active = false) {
    DrawRectangleRounded(rect, 0.16f, 5,
                         active ? Color{45, 81, 88, 255}
                                : Color{36, 43, 58, 255});
    DrawRectangleRoundedLinesEx(rect, 0.16f, 5, 1.7f, tint);
    const auto headingSize = MeasureTextEx(font, name, 13.0f, 1.0f);
    const auto keySize = MeasureTextEx(font, keys, 10.0f, 1.0f);
    Label(font, name, rect.x + (rect.width - headingSize.x) / 2.0f,
          rect.y + 2.0f, 13.0f, Color{232, 238, 245, 255});
    Label(font, keys, rect.x + (rect.width - keySize.x) / 2.0f,
          rect.y + 20.0f, 10.0f, Color{196, 213, 235, 255});
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

void DrawPreviewGrid(Font font, const core::ChartEditor& draft, double previewBeat) {
    const auto& chart = draft.Data().route;
    const double bounded = std::clamp(previewBeat,
                                      static_cast<double>(chart.firstBeat),
                                      static_cast<double>(chart.lengthBeats));
    const EditorGrid grid(chart, bounded);
    const RouteCanvas canvas{grid.left, grid.top,
                              grid.step / chart.ticksPerBeat, grid.origin};

    // The grid is retained only in ROUTE; it is a scrolling view over the
    // limitless world, not a wall that routes bounce from or teleport across.
    const float x0 = kGridCenterX - kGridMaxSpan * 0.5f;
    const float y0 = kGridCenterY - kGridMaxSpan * 0.5f;
    const float x1 = x0 + kGridMaxSpan, y1 = y0 + kGridMaxSpan;
    const float scale = std::min(static_cast<float>(GetScreenWidth()) / kVirtualWidth,
                                 static_cast<float>(GetScreenHeight()) / kVirtualHeight);
    const float ox = (GetScreenWidth() - kVirtualWidth * scale) * 0.5f;
    const float oy = (GetScreenHeight() - kVirtualHeight * scale) * 0.5f;
    BeginScissorMode(static_cast<int>(std::floor(ox + x0 * scale)),
                     static_cast<int>(std::floor(oy + y0 * scale)),
                     static_cast<int>(std::ceil(kGridMaxSpan * scale)),
                     static_cast<int>(std::ceil(kGridMaxSpan * scale)));

    const int halfCells = static_cast<int>(std::ceil(kGridMaxSpan / grid.step)) + 2;
    const int cx = static_cast<int>(std::floor(
        chart.start.x + (grid.origin.x - chart.start.x) / chart.ticksPerBeat));
    const int cy = static_cast<int>(std::floor(
        chart.start.y + (grid.origin.y - chart.start.y) / chart.ticksPerBeat));
    for (int i = -halfCells; i <= halfCells; ++i) {
        const float x = grid.Point(core::Cell{cx + i, cy}).x;
        if (x >= x0 - 1.0f && x <= x1 + 1.0f)
            DrawLineEx({x, y0}, {x, y1}, 1.25f, Color{51, 62, 79, 255});
        const float y = grid.Point(core::Cell{cx, cy + i}).y;
        if (y >= y0 - 1.0f && y <= y1 + 1.0f)
            DrawLineEx({x0, y}, {x1, y}, 1.25f, Color{51, 62, 79, 255});
    }

    const Vector2 marker = DrawRouteTrajectory(
        chart, bounded, canvas, RouteRenderMode::Editor);
    const float dot = std::min(7.0f, grid.step * 0.25f);
    for (int j = -halfCells; j <= halfCells; ++j) {
        for (int i = -halfCells; i <= halfCells; ++i) {
            const Vector2 position = grid.Point(core::Cell{cx + i, cy + j});
            if (position.x < x0 || position.x > x1 ||
                position.y < y0 || position.y > y1) continue;
            DrawCircleV(position, dot, Color{105, 127, 148, 255});
            DrawCircleV(position, dot * 0.41f, Color{207, 221, 234, 255});
        }
    }
    DrawRouteJunctions(chart, bounded, canvas, RouteRenderMode::Editor);
    const Vector2 start = grid.Point(chart.start);
    const float markerRadius = std::min(13.0f, grid.step * 0.39f);
    if (start.x >= x0 && start.x <= x1 && start.y >= y0 && start.y <= y1) {
        DrawCircleV(start, markerRadius * 1.32f, Color{245, 203, 112, 48});
        DrawCircleLines(static_cast<int>(start.x), static_cast<int>(start.y),
                        markerRadius * 1.36f, Color{246, 209, 112, 255});
    }
    DrawCircleV(marker, markerRadius * 1.7f, Color{112, 219, 216, 48});
    DrawCircleV(marker, markerRadius, Color{124, 229, 225, 255});
    DrawCircleV(marker, markerRadius * 0.32f, Color{246, 251, 252, 255});
    EndScissorMode();

    Label(font, TextFormat("BEAT %.2f / %d", bounded, chart.lengthBeats - 1),
          52, 405, 15, Color{158, 225, 212, 255});
    Label(font, TextFormat("START %d/%d", chart.start.x, chart.start.y),
          266, 405, 15, Color{247, 213, 131, 255});
}

void DrawTimeline(Font font, const core::ChartEditor& draft,
                  const EditorUiState& ui, double previewBeat) {
    const auto& chart = draft.Data().route;
    const double anchor = ui.draggingTimeline
        ? ui.dragViewBeat : ViewBeat(draft, previewBeat);
    const float pixels = PixelsPerBeat(ui);

    if (ui.HasRange()) {
        Label(font, TextFormat("RANGE %d-%d", ui.rangeFirst, ui.rangeLast),
              50, 422, 17, Color{239, 207, 131, 255});
    } else {
        Label(font, TextFormat("TIMELINE  %.2f / %d", anchor, chart.lengthBeats),
              50, 422, 17, Color{209, 222, 234, 255});
    }
    Label(font, "ARROWS 1  CTRL+ARROWS 4", 328, 423, 13,
          Color{164, 188, 208, 255});
    Label(font, TextFormat("%d PX/BEAT", static_cast<int>(pixels)),
          693, 423, 13, Color{174, 205, 229, 255});
    Button(font, kZoomOut, "-", Color{125, 167, 205, 255});
    Button(font, kZoomIn, "+", Color{125, 167, 205, 255});

    const Rectangle strip{kTimelineX, kTimelineY, kTimelineWidth, kTimelineHeight};
    DrawRectangleRounded(strip, 0.15f, 5, Color{34, 44, 59, 255});
    DrawRectangleRec({kTimelineX + 2.0f, kTimelineY + 2.0f,
                      kPlayheadX - kTimelineX - 2.0f, kTimelineHeight - 4.0f},
                     Color{41, 62, 70, 255}); // Past the fixed cursor.
    if (ui.tab == EditorTab::Route && ui.HasRange()) {
        const float a = kPlayheadX +
            static_cast<float>((ui.rangeFirst - 0.5 - anchor) * pixels);
        const float b = kPlayheadX +
            static_cast<float>((ui.rangeLast + 0.5 - anchor) * pixels);
        const float x1 = std::max(a, kTimelineX + 2.0f);
        const float x2 = std::min(b, kTimelineX + kTimelineWidth - 2.0f);
        if (x2 > x1) {
            DrawRectangleRec({x1, kTimelineY + 2.0f, x2 - x1,
                              kTimelineHeight - 4.0f},
                             Color{187, 160, 231, 96});
        }
    }
    DrawLineEx({kTimelineX + 4.0f, 477.0f},
               {kTimelineX + kTimelineWidth - 4.0f, 477.0f},
               2.0f, Color{98, 125, 150, 255});

    // Visible beat ticks are projected from a continuous (possibly fractional)
    // beat. The ruler slides every frame; it never jumps at 15 -> 16.
    const int first = std::max(chart.firstBeat, static_cast<int>(std::floor(
        anchor + (kTimelineX - kPlayheadX) / pixels)) - 1);
    const int last = std::min(chart.lengthBeats - 1,
        static_cast<int>(std::ceil(
            anchor + (kTimelineX + kTimelineWidth - kPlayheadX) / pixels)) + 1);
    const int barLabelStride = pixels < 20.0f ? 8 : 4;

    for (int beat = first; beat <= last; ++beat) {
        const float x = kPlayheadX +
                        static_cast<float>((static_cast<double>(beat) - anchor) * pixels);
        if (x < kTimelineX + 5.0f || x > kTimelineX + kTimelineWidth - 5.0f)
            continue;
        const bool bar = beat % 4 == 0;
        DrawLineEx({x, bar ? 462.0f : 470.0f}, {x, 486.0f},
                   bar ? 2.5f : 1.25f,
                   bar ? Color{199, 220, 234, 255}
                       : Color{113, 141, 163, 255});
        if (bar && beat % barLabelStride == 0 &&
            x < kTimelineX + kTimelineWidth - 55.0f) {
            Label(font, TextFormat("%d", beat), x + 4.0f, 450.0f, 13,
                  Color{195, 215, 229, 255});
        }
    }

    // Draw turn symbols from the actual chart: left=teal, right=violet.
    // Iterate authored turns (already sorted), stopping after the view.
    for (const auto& event : chart.turns) {
        if (event.beat < first) continue;
        if (event.beat > last) break;
        const float x = kPlayheadX +
                        static_cast<float>((static_cast<double>(event.beat) - anchor) * pixels);
        if (x < kTimelineX + 8.0f || x > kTimelineX + kTimelineWidth - 8.0f)
            continue;
        const bool left = event.direction == core::Turn::Left ||
                          event.direction == core::Turn::Left45 ||
                          event.direction == core::Turn::Left135;
        const bool narrow = pixels < 26.0f;
        const float width = narrow ? 12.0f : std::min(pixels - 3.0f, 32.0f);
        DrawRectangleRec({x - width * 0.5f, 481.0f, width, 15.0f},
                         left ? Color{96, 220, 193, 255}
                              : Color{175, 155, 236, 255});
        if (!narrow) {
            const char* angle = event.direction == core::Turn::Left45 ||
                                event.direction == core::Turn::Right45 ? "45"
                              : event.direction == core::Turn::Left135 ||
                                event.direction == core::Turn::Right135 ? "135" : "90";
            Label(font, TextFormat("%s%s", left ? "L" : "R", angle),
                  x - width * 0.5f + 2.0f, 482.0f,
                  10.0f, Color{22, 26, 36, 255});
        }
    }

    // Last, draw the stationary playhead *over* the moving ruler/events.
    DrawRectangleRec({kPlayheadX - 4.0f, kTimelineY + 1.0f,
                      8.0f, kTimelineHeight - 2.0f},
                     Color{241, 206, 118, 255});
    DrawCircleV({kPlayheadX, 477.0f}, 6.0f,
                Color{253, 246, 215, 255});
}
} // namespace

void RenderChartEditor(Font font, const core::ChartEditor& draft,
                       const EditorUiState& ui, const std::string& songName,
                       bool hasAudio, bool previewPlaying, bool previewPaused,
                       double previewBeat, double playbackSpeed) {
    const auto& chart = draft.Data().route;
    Label(font, "CHART EDITOR", 49, 77, 26, Color{194, 236, 231, 255});
    Button(font, kTabRoute, "ROUTE", Color{126, 219, 209, 255},
           ui.tab == EditorTab::Route);
    Button(font, kTabAudio, "AUDIO", Color{131, 190, 237, 255},
           ui.tab == EditorTab::Audio);
    Button(font, kTabField, "FIELD", Color{239, 203, 129, 255},
           ui.tab == EditorTab::Field);

    // Transport and timeline persist across all editor tabs.
    Button(font, kSong, "LOAD SONG  O", Color{97, 222, 193, 255});
    Button(font, kPlay, previewPlaying && !previewPaused ? "PAUSE SPACE" : "PLAY SPACE",
           Color{137, 211, 235, 255});
    Button(font, kStop, "RESTART R", Color{211, 178, 137, 255});
    Button(font, kTest, "TEST T", Color{188, 176, 232, 255});
    Label(font, hasAudio ? Truncate(songName, 30).c_str()
                         : "NO SONG - LOAD SONG",
          626, 127, 15,
          hasAudio ? Color{178, 227, 222, 255} : Color{242, 181, 164, 255});

    if (ui.tab == EditorTab::Route) {
        Label(font, TextFormat("ROUTE  /  %d x %d EDITOR GRID",
                              chart.gridWidth, chart.gridHeight),
              51, 166, 17, Color{188, 215, 227, 255});
        DrawPreviewGrid(font, draft, previewBeat);
        Label(font, "START HEADING Q/E 45", 744, 177, 13,
              Color{172, 199, 218, 255});
        Button(font, kHeading, HeadingName(chart.initialHeading),
               Color{119, 229, 188, 255});
        const auto* turn = draft.SelectedTurn();
        const auto incoming = core::IncomingHeadingAt(chart, draft.SelectedBeat());
        const auto outgoing = turn ? core::HeadingAfterTurn(incoming, turn->direction)
                                   : incoming;
        Label(font, TextFormat("BEAT %d / %s", draft.SelectedBeat(),
                              turn ? core::TurnName(turn->direction) : "STRAIGHT"),
              744, 250, 12, Color{216, 232, 239, 255});
        const char* padNames[9] = {"UL", "U", "UR", "L", "DEL", "R",
                                   "DL", "D", "DR"};
        const char* padKeys[9] = {"D+J", "J", "J+K", "D", "BKSP", "K",
                                  "D+F", "F", "F+K"};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col) {
                const auto heading = DirectionPadHeading(row, col);
                DirectionButton(font, DirectionPadBounds(row, col),
                                padNames[row * 3 + col], padKeys[row * 3 + col],
                                !heading ? Color{224, 150, 148, 255}
                                  : core::IsDiagonal(*heading)
                                    ? Color{186, 160, 235, 255}
                                    : Color{110, 220, 198, 255},
                                heading ? (turn && *heading == outgoing)
                                        : turn != nullptr);
            }
        Label(font, "DFJK: TWO KEYS = DIAGONAL", 744, 398, 11,
              Color{165, 198, 213, 255});
        Label(font, "PATTERN  /  DRAG RULER", 52, 188, 14,
              Color{193, 215, 226, 255});
        Button(font, kCopyTurns, "COPY CTRL+C", Color{126, 190, 221, 255});
        Button(font, kPasteTurns, "PASTE CTRL+V", Color{127, 224, 199, 255});
        Button(font, kInvertTurns, "FLIP L/R  I", Color{190, 168, 229, 255});
        Button(font, kCutTurns, "CUT CTRL+X", Color{216, 166, 155, 255});
        Button(font, kUndoEdit, "UNDO", Color{169, 188, 217, 255});
        Button(font, kRedoEdit, "REDO", Color{169, 188, 217, 255});
        Label(font, "PLAYBACK SPEED", 618, 185, 14,
              Color{182, 207, 227, 255});
        const char* speeds[5] = {"100%", "75%", "50%", "25%", "10%"};
        for (int i = 0; i < 5; ++i) {
            Button(font, RouteSpeedBounds(i), speeds[i],
                   Color{125, 183, 222, 255},
                   std::abs(playbackSpeed - kSpeeds[i]) < 0.001);
        }
    } else if (ui.tab == EditorTab::Audio) {
        Label(font, "AUDIO  /  CHART TIMING", 92, 169, 20,
              Color{182, 218, 235, 255});
        DrawTextField(font, kTitle, "CHART TITLE (TYPE)", draft.Data().title,
                      EditorField::Title, ui);
        char bpm[40]{}, offset[40]{};
        std::snprintf(bpm, sizeof(bpm), "%.3f", chart.bpm);
        std::snprintf(offset, sizeof(offset), "%.3f",
                      draft.Data().offsetSeconds * 1000.0);
        DrawTextField(font, kBpm, "BPM (TYPE)", bpm, EditorField::Bpm, ui);
        DrawTextField(font, kOffset, "OFFSET IN MILLISECONDS (TYPE)",
                      offset, EditorField::OffsetMs, ui);
        Label(font, "PRACTICE SPEED: SET ON ROUTE (KEYS 1-5 WORK HERE TOO)",
              92, 374, 16, Color{172, 199, 218, 255});
    } else {
        Label(font, "FIELD  /  EDITOR GRID VIEW", 104, 169, 20,
              Color{239, 210, 159, 255});
        Label(font, "NOT A MUSIC TIME SIGNATURE", 548, 172, 14,
              Color{161, 187, 210, 255});
        DrawTextField(font, kGridSize, "EDITOR GRID  W/H  (2..16)",
                      std::to_string(chart.gridWidth) + "/" +
                      std::to_string(chart.gridHeight), EditorField::GridSize, ui);
        DrawTextField(font, kStartCell, "START CELL X/Y  (FROM 0)",
                      std::to_string(chart.start.x) + "/" +
                      std::to_string(chart.start.y), EditorField::StartCell, ui);
        Label(font, draft.HasSongDuration()
              ? TextFormat("SONG TIMELINE: %d TO %d (AUTO FROM BPM / OFFSET)",
                           chart.firstBeat, chart.lengthBeats - 1)
              : "SONG TIMELINE: LOAD AUDIO TO CALCULATE",
              104, 329, 15, Color{188, 218, 228, 255});
        Label(font, "START HEADING  Q/E 45 DEG", 519, 299, 16,
              Color{172, 199, 218, 255});
        Button(font, kFieldHeading, HeadingName(chart.initialHeading),
               Color{119, 229, 188, 255});
        Label(font, "BEATS COVER THE ENTIRE SONG, INCLUDING BEFORE OFFSET.",
              104, 384, 14, Color{169, 204, 218, 255});
        Label(font, "GRID SIZE IS DISPLAY ONLY: WORLD NEVER WRAPS.",
              104, 403, 13, Color{226, 197, 146, 255});
    }

    DrawTimeline(font, draft, ui, previewBeat);
    const std::string message = ui.feedback.empty()
        ? (ui.EditingText() ? "TYPE VALUE, ENTER CONFIRMS, ESC CANCELS"
                             : "CTRL+S SAVE  CTRL+O OPEN  H HOME  F11 FULLSCREEN")
        : ui.feedback;
    Label(font, Truncate(message, 90).c_str(), 50, 508, 14,
          ui.feedback.empty() ? Color{177, 198, 218, 255}
              : ui.feedbackError ? Color{238, 166, 154, 255}
                                 : Color{142, 232, 207, 255});
}

} // namespace locus::app
