#include "app/chart_editor_ui.h"

#include "core/route.h"
#include "core/route_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

namespace locus::app {
namespace {
constexpr float kVirtualWidth = 960.0f, kVirtualHeight = 540.0f;
constexpr float kGridCenterX = 448.0f, kGridCenterY = 282.0f;
constexpr float kGridMaxSpan = 240.0f;
constexpr float kTimelineX = 50.0f, kTimelineY = 447.0f;
constexpr float kTimelineWidth = 860.0f, kTimelineHeight = 52.0f;
// Fixed playhead at one third of the ruler; the fractional-beat ruler moves.
constexpr float kPlayheadX = kTimelineX + kTimelineWidth / 3.0f;
constexpr float kZoomPixelsPerBeat[7] = {12, 18, 26, 36, 52, 76, 104};
constexpr int kZoomMax = 6;
constexpr Rectangle kZoomOut{807, 416, 34, 27};
constexpr Rectangle kZoomIn{863, 416, 34, 27};

float PixelsPerBeat(const EditorUiState& ui) {
    return kZoomPixelsPerBeat[std::clamp(ui.timelineZoomIndex, 0, kZoomMax)];
}

double ViewBeat(const core::ChartEditor& draft, double activeBeat) {
    return std::isfinite(activeBeat) && activeBeat >= 0.0
        ? std::clamp(activeBeat, 0.0,
                     static_cast<double>(draft.Data().route.lengthBeats))
        : static_cast<double>(draft.SelectedBeat());
}

constexpr Rectangle kTabRoute{388, 73, 155, 35};
constexpr Rectangle kTabAudio{552, 73, 155, 35};
constexpr Rectangle kTabField{716, 73, 155, 35};

constexpr Rectangle kSong{49, 115, 162, 37};
constexpr Rectangle kPlay{220, 115, 157, 37};
constexpr Rectangle kStop{386, 115, 115, 37};
constexpr Rectangle kTest{510, 115, 102, 37};

constexpr Rectangle kHeading{744, 201, 168, 40};
constexpr Rectangle kLeftTurn{744, 271, 168, 37};
constexpr Rectangle kRightTurn{744, 314, 168, 37};
constexpr Rectangle kRemoveTurn{744, 357, 168, 37};
constexpr Rectangle kCopyTurns{52, 213, 173, 31};
constexpr Rectangle kPasteTurns{52, 248, 173, 31};
constexpr Rectangle kInvertTurns{52, 283, 173, 31};
constexpr Rectangle kCutTurns{52, 318, 173, 31};
constexpr Rectangle kUndoEdit{52, 354, 83, 31};
constexpr Rectangle kRedoEdit{142, 354, 83, 31};

constexpr Rectangle kTitle{92, 205, 768, 43};
constexpr Rectangle kBpm{92, 290, 350, 43};
constexpr Rectangle kOffset{510, 290, 350, 43};
constexpr Rectangle kGridSize{104, 225, 330, 47};
constexpr Rectangle kStartCell{519, 225, 330, 47};
constexpr Rectangle kLength{104, 321, 330, 47};
constexpr Rectangle kFieldHeading{519, 321, 330, 47};

constexpr double kSpeeds[5] = {1.0, 0.75, 0.5, 0.25, 0.1};

bool Hit(Rectangle bounds, Vector2 pos) { return CheckCollisionPointRec(pos, bounds); }

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

struct EditorGrid {
    float left = 0.0f, top = 0.0f, step = 1.0f;

    explicit EditorGrid(const core::Chart& chart) {
        const int maxSteps = std::max(chart.gridWidth - 1, chart.gridHeight - 1);
        step = kGridMaxSpan / static_cast<float>(std::max(1, maxSteps));
        left = kGridCenterX - (chart.gridWidth - 1) * step * 0.5f;
        top = kGridCenterY - (chart.gridHeight - 1) * step * 0.5f;
    }
    Vector2 Point(core::GridPoint point) const {
        return {left + static_cast<float>(point.x) * step,
                top + static_cast<float>(point.y) * step};
    }
    Vector2 Point(core::Cell point) const {
        return Point(core::GridPoint{static_cast<double>(point.x),
                                    static_cast<double>(point.y)});
    }
};

EditorTab ClickedTab(Vector2 mouse, EditorTab previous) {
    if (Hit(kTabRoute, mouse)) return EditorTab::Route;
    if (Hit(kTabAudio, mouse)) return EditorTab::Audio;
    if (Hit(kTabField, mouse)) return EditorTab::Field;
    return previous;
}

const char* HeadingName(core::Heading heading) {
    switch (heading) {
        case core::Heading::Up: return "UP";
        case core::Heading::Right: return "RIGHT";
        case core::Heading::Down: return "DOWN";
        case core::Heading::Left: return "LEFT";
    }
    return "?";
}

const char* FieldName(EditorField field) {
    switch (field) {
        case EditorField::Title: return "TITLE";
        case EditorField::Bpm: return "BPM";
        case EditorField::OffsetMs: return "OFFSET (MS)";
        case EditorField::Length: return "BEATS";
        case EditorField::GridSize: return "GRID SIZE (W/H)";
        case EditorField::StartCell: return "START (X/Y)";
        case EditorField::None: break;
    }
    return "";
}

Rectangle FieldBounds(EditorField field) {
    switch (field) {
        case EditorField::Title: return kTitle;
        case EditorField::Bpm: return kBpm;
        case EditorField::OffsetMs: return kOffset;
        case EditorField::Length: return kLength;
        case EditorField::GridSize: return kGridSize;
        case EditorField::StartCell: return kStartCell;
        case EditorField::None: break;
    }
    return {0, 0, 0, 0};
}

EditorField ClickedField(Vector2 mouse, EditorTab tab) {
    if (tab == EditorTab::Audio) {
        for (const auto field : {EditorField::Title, EditorField::Bpm,
                                  EditorField::OffsetMs}) {
            if (Hit(FieldBounds(field), mouse)) return field;
        }
    } else if (tab == EditorTab::Field) {
        for (const auto field : {EditorField::GridSize, EditorField::StartCell,
                                  EditorField::Length}) {
            if (Hit(FieldBounds(field), mouse)) return field;
        }
    }
    return EditorField::None;
}

std::string InitialValue(const core::ChartEditor& draft, EditorField field) {
    char text[100]{};
    switch (field) {
        case EditorField::Title: return draft.Data().title;
        case EditorField::Bpm:
            std::snprintf(text, sizeof(text), "%.3f", draft.Data().route.bpm);
            return text;
        case EditorField::OffsetMs:
            std::snprintf(text, sizeof(text), "%.3f", draft.Data().offsetSeconds * 1000.0);
            return text;
        case EditorField::Length:
            return std::to_string(draft.Data().route.lengthBeats);
        case EditorField::GridSize:
            return std::to_string(draft.Data().route.gridWidth) + "/" +
                   std::to_string(draft.Data().route.gridHeight);
        case EditorField::StartCell:
            return std::to_string(draft.Data().route.start.x) + "/" +
                   std::to_string(draft.Data().route.start.y);
        default: return "";
    }
}

void BeginField(EditorUiState& ui, const core::ChartEditor& draft, EditorField field) {
    ui.active = field;
    ui.buffer = InitialValue(draft, field);
    ui.replaceOnType = true; // The first character replaces the shown number.
    ui.feedback = "";
}

bool ParseNumeric(const std::string& string, double& result) {
    if (string.empty()) return false;
    char* end = nullptr;
    result = std::strtod(string.c_str(), &end);
    return end && end != string.c_str() && *end == '\0' && std::isfinite(result);
}

// A field size like 8/8 (or 8x8) is two game-field dimensions,
// not a musical time signature. Coordinates are zero-based.
bool ParsePair(const std::string& value, int& first, int& second) {
    if (value.empty()) return false;
    char* middle = nullptr;
    const long a = std::strtol(value.c_str(), &middle, 10);
    if (middle == value.c_str() ||
        (*middle != '/' && *middle != 'x' && *middle != 'X')) return false;
    char* end = nullptr;
    const long b = std::strtol(middle + 1, &end, 10);
    if (end == middle + 1 || *end != '\0' ||
        a < 0 || a > std::numeric_limits<int>::max() ||
        b < 0 || b > std::numeric_limits<int>::max()) return false;
    first = static_cast<int>(a);
    second = static_cast<int>(b);
    return true;
}

bool CommitField(EditorUiState& ui, core::ChartEditor& draft) {
    if (!ui.EditingText()) return true;
    const auto field = ui.active;
    bool valid = false;
    if (field == EditorField::Title) {
        valid = draft.SetTitle(ui.buffer);
    } else if (field == EditorField::GridSize || field == EditorField::StartCell) {
        int x = 0, y = 0;
        if (ParsePair(ui.buffer, x, y)) {
            if (field == EditorField::GridSize) valid = draft.SetGridSize(x, y);
            else valid = draft.SetStart({x, y}) ||
                         (draft.Data().route.start.x == x &&
                          draft.Data().route.start.y == y);
        }
    } else {
        double value = 0.0;
        if (ParseNumeric(ui.buffer, value)) {
            if (field == EditorField::Bpm) valid = draft.SetBpm(value);
            if (field == EditorField::OffsetMs) valid = draft.SetOffsetMs(value);
            if (field == EditorField::Length && value >= 1.0 &&
                value <= 65536.0 && std::floor(value) == value)
                valid = draft.SetLength(static_cast<int>(value));
        }
    }
    if (!valid) {
        ui.feedback = std::string("INVALID ") + FieldName(field) +
                      " - CHECK VALUE AND TRY ENTER";
        return false;
    }
    ui.active = EditorField::None;
    ui.buffer.clear();
    ui.replaceOnType = false;
    ui.feedback.clear();
    return true;
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
    const EditorGrid grid(chart);
    for (int y = 0; y < chart.gridHeight; ++y) {
        DrawLineEx(grid.Point(core::Cell{0, y}),
                   grid.Point(core::Cell{chart.gridWidth - 1, y}),
                   1.5f, Color{51, 62, 79, 255});
    }
    for (int x = 0; x < chart.gridWidth; ++x) {
        DrawLineEx(grid.Point(core::Cell{x, 0}),
                   grid.Point(core::Cell{x, chart.gridHeight - 1}),
                   1.5f, Color{51, 62, 79, 255});
    }

    const double bounded = std::clamp(previewBeat, 0.0,
                                      static_cast<double>(chart.lengthBeats));
    const int beat = std::min(static_cast<int>(std::floor(bounded)),
                              chart.lengthBeats - 1);
    const double progress = std::clamp(bounded - beat, 0.0, 1.0);
    const core::RouteStep current = core::SampleRouteStep(chart, beat);
    const core::StepView whole = core::ViewRouteStep(current, progress);
    const core::StepView trace = core::TraceRouteStep(current, progress);

    const auto drawSegments = [&](const core::StepView& view, Color tint, float width) {
        const float scale = std::clamp(grid.step / 80.0f, 0.4f, 1.0f);
        for (int i = 0; i < view.segmentCount; ++i) {
            DrawLineEx(grid.Point(view.segments[i].from),
                       grid.Point(view.segments[i].to), width * scale, tint);
        }
    };
    if (beat + 1 < chart.lengthBeats) {
        const core::RouteStep next = core::SampleRouteStep(chart, beat + 1);
        drawSegments(core::ViewRouteStep(next, 1.0),
                     Color{88, 153, 199, 110}, 3.0f);
    }
    drawSegments(whole, Color{95, 179, 230, 200}, 4.0f);
    drawSegments(trace, Color{110, 235, 218, 255}, 5.5f);

    const float dot = std::min(8.0f, grid.step * 0.27f);
    for (int y = 0; y < chart.gridHeight; ++y) {
        for (int x = 0; x < chart.gridWidth; ++x) {
            const Vector2 position = grid.Point(core::Cell{x, y});
            DrawCircleV(position, dot, Color{105, 127, 148, 255});
            DrawCircleV(position, dot * 0.41f, Color{207, 221, 234, 255});
        }
    }
    const Vector2 start = grid.Point(chart.start);
    const float markerRadius = std::min(13.0f, grid.step * 0.39f);
    DrawCircleV(start, markerRadius * 1.32f, Color{245, 203, 112, 48});
    DrawCircleLines(static_cast<int>(start.x), static_cast<int>(start.y),
                    markerRadius * 1.36f, Color{246, 209, 112, 255});
    const Vector2 marker = grid.Point(trace.marker);
    DrawCircleV(marker, markerRadius * 1.7f, Color{112, 219, 216, 48});
    DrawCircleV(marker, markerRadius, Color{124, 229, 225, 255});
    DrawCircleV(marker, markerRadius * 0.32f, Color{246, 251, 252, 255});

    Label(font, TextFormat("BEAT %.2f / %d", bounded, chart.lengthBeats),
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
    const int first = std::max(0, static_cast<int>(std::floor(
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
        const bool left = event.direction == core::Turn::Left;
        DrawRectangleRec({x - 6.0f, 481.0f, 12.0f, 15.0f},
                         left ? Color{96, 220, 193, 255}
                              : Color{175, 155, 236, 255});
        if (pixels >= 26.0f) {
            Label(font, left ? "L" : "R", x - 4.0f, 480.0f, 12,
                  Color{22, 26, 36, 255});
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

Vector2 EditorVirtualMouse(Vector2 physical, int screenWidth, int screenHeight) {
    const float scale = std::min(static_cast<float>(screenWidth) / kVirtualWidth,
                                 static_cast<float>(screenHeight) / kVirtualHeight);
    if (scale <= 0.0f) return {-1, -1};
    const float x = (screenWidth - kVirtualWidth * scale) / 2.0f;
    const float y = (screenHeight - kVirtualHeight * scale) / 2.0f;
    return {(physical.x - x) / scale, (physical.y - y) / scale};
}

EditorControls PollEditorControls() {
    EditorControls input;
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    input.shiftHeld = shift;
    input.copyRange = control && IsKeyPressed(KEY_C);
    input.cutRange = control && IsKeyPressed(KEY_X);
    input.pasteRange = control && IsKeyPressed(KEY_V);
    input.invertRange = !control && IsKeyPressed(KEY_I);
    input.undo = control && !shift && IsKeyPressed(KEY_Z);
    input.redo = (control && IsKeyPressed(KEY_Y)) ||
                 (control && shift && IsKeyPressed(KEY_Z));
    input.shiftRangePrevious = control && shift && IsKeyPressed(KEY_LEFT);
    input.shiftRangeNext = control && shift && IsKeyPressed(KEY_RIGHT);
    input.zoomIn = IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD) ||
                   IsKeyPressed(KEY_RIGHT_BRACKET);
    input.zoomOut = IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT) ||
                    IsKeyPressed(KEY_LEFT_BRACKET);
    input.previousBeat = IsKeyPressed(KEY_LEFT) && !shift && !control;
    input.nextBeat = IsKeyPressed(KEY_RIGHT) && !shift && !control;
    input.previousBar = IsKeyPressed(KEY_LEFT) && control && !shift;
    input.nextBar = IsKeyPressed(KEY_RIGHT) && control && !shift;
    input.movePrevious = IsKeyPressed(KEY_LEFT) && shift && !control;
    input.moveNext = IsKeyPressed(KEY_RIGHT) && shift && !control;
    input.addLeft = IsKeyPressed(KEY_F);
    input.addRight = IsKeyPressed(KEY_J);
    input.removeTurn = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE);
    input.headingLeft = IsKeyPressed(KEY_Q);
    input.headingRight = IsKeyPressed(KEY_E);
    input.previewToggle = IsKeyPressed(KEY_SPACE);
    input.previewRestart = IsKeyPressed(KEY_R);
    input.enter = IsKeyPressed(KEY_ENTER);
    input.escape = IsKeyPressed(KEY_ESCAPE);
    input.backspace = IsKeyPressed(KEY_BACKSPACE);
    input.tab = IsKeyPressed(KEY_TAB);
    input.speedIndex = IsKeyPressed(KEY_ONE) ? 0
                     : IsKeyPressed(KEY_TWO) ? 1
                     : IsKeyPressed(KEY_THREE) ? 2
                     : IsKeyPressed(KEY_FOUR) ? 3
                     : IsKeyPressed(KEY_FIVE) ? 4 : -1;
    input.mouseClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.mouseHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.mouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    input.mouse = EditorVirtualMouse(GetMousePosition(), GetScreenWidth(),
                                     GetScreenHeight());
    input.mouseWheel = GetMouseWheelMove();
    if (control && input.mouseWheel != 0.0f) {
        input.zoomIn |= input.mouseWheel > 0;
        input.zoomOut |= input.mouseWheel < 0;
        input.mouseWheel = 0.0f;
    }
    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        int bytes = 0;
        const char* utf8 = CodepointToUTF8(codepoint, &bytes);
        if (utf8 && bytes > 0) input.typedUtf8.append(utf8, bytes);
        codepoint = GetCharPressed();
    }
    if (control && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (clipboard) input.typedUtf8 = clipboard; // Ctrl+V pastes the full value.
    }
    return input;
}

EditorUiAction ApplyEditorControls(core::ChartEditor& draft, EditorUiState& ui,
                                  const EditorControls& input) {
    EditorUiAction action;
    const auto notice = [&](const char* message, bool error = false) {
        ui.feedback = message;
        ui.feedbackError = error;
    };

    // Keep all editor shortcuts away from active text fields.
    if (ui.EditingText()) {
        if (input.escape) {
            ui.active = EditorField::None;
            ui.buffer.clear();
            ui.replaceOnType = false;
            ui.feedback.clear();
            ui.feedbackError = false;
        } else if (input.backspace) {
            if (ui.replaceOnType) {
                ui.buffer.clear();
                ui.replaceOnType = false;
            } else if (!ui.buffer.empty()) {
                std::size_t pos = ui.buffer.size() - 1;
                while (pos > 0 &&
                       (static_cast<unsigned char>(ui.buffer[pos]) & 0xc0) == 0x80)
                    --pos;
                ui.buffer.erase(pos);
            }
        }
        if (!input.typedUtf8.empty()) {
            if (ui.replaceOnType) {
                ui.buffer.clear();
                ui.replaceOnType = false;
            }
            for (unsigned char c : input.typedUtf8) {
                const bool pairField = ui.active == EditorField::GridSize ||
                                       ui.active == EditorField::StartCell;
                if (ui.active == EditorField::Title ||
                    (pairField && (c == '/' || c == 'x' || c == 'X')) ||
                    c == '.' || c == ',' || c == '-' || (c >= '0' && c <= '9')) {
                    if (ui.buffer.size() < 100)
                        ui.buffer.push_back(static_cast<char>(
                            c == ',' && ui.active != EditorField::Title ? '.' : c));
                }
            }
        }
        if (input.enter || input.tab) {
            const bool committed = CommitField(ui, draft);
            action.changedChart = committed;
            if (committed && input.tab) {
                const EditorField next = ui.tab == EditorTab::Audio
                    ? EditorField::Bpm : EditorField::GridSize;
                BeginField(ui, draft, next);
            }
        } else if (input.mouseClick) {
            const EditorField target = ClickedField(input.mouse, ui.tab);
            const EditorTab nextTab = ClickedTab(input.mouse, ui.tab);
            if (target != ui.active || nextTab != ui.tab) {
                const bool committed = CommitField(ui, draft);
                action.changedChart = committed;
                if (committed) {
                    ui.tab = nextTab;
                    if (target != EditorField::None && nextTab == ui.tab)
                        BeginField(ui, draft, target);
                }
            }
        }
        return action;
    }

    // Undo/redo also cover non-pattern edits (BPM, grid size, individual turns).
    if (input.undo || input.redo) {
        const bool ok = input.undo ? draft.Undo() : draft.Redo();
        action.changedChart = ok;
        notice(ok ? (input.undo ? "UNDO APPLIED" : "REDO APPLIED")
                  : (input.undo ? "NOTHING TO UNDO" : "NOTHING TO REDO"), !ok);
        if (ok && ui.HasRange()) {
            if (ui.rangeLast >= draft.Data().route.lengthBeats)
                ui.rangeFirst = ui.rangeLast = -1;
        }
        return action;
    }

    bool copy = input.copyRange && ui.tab == EditorTab::Route;
    bool cut = input.cutRange && ui.tab == EditorTab::Route;
    bool paste = input.pasteRange && ui.tab == EditorTab::Route;
    bool invert = input.invertRange && ui.tab == EditorTab::Route;
    bool deleteTurns = input.removeTurn && ui.tab == EditorTab::Route;

    if (input.mouseClick) {
        ui.draggingTimeline = false;
        const Vector2 mouse = input.mouse;
        const EditorTab nextTab = ClickedTab(mouse, ui.tab);
        if (nextTab != ui.tab) {
            ui.tab = nextTab;
            return action;
        }
        const EditorField selected = ClickedField(mouse, ui.tab);
        if (selected != EditorField::None) {
            BeginField(ui, draft, selected);
            return action;
        }

        if (Hit(kSong, mouse)) action.chooseAudio = true;
        else if (Hit(kPlay, mouse)) action.previewToggle = true;
        else if (Hit(kStop, mouse)) action.previewRestart = true;
        else if (Hit(kTest, mouse)) action.testPlay = true;
        else if (Hit(kZoomOut, mouse))
            ui.timelineZoomIndex = std::max(0, ui.timelineZoomIndex - 1);
        else if (Hit(kZoomIn, mouse))
            ui.timelineZoomIndex = std::min(kZoomMax, ui.timelineZoomIndex + 1);
        else if (ui.tab == EditorTab::Route && Hit(kCopyTurns, mouse)) copy = true;
        else if (ui.tab == EditorTab::Route && Hit(kPasteTurns, mouse)) paste = true;
        else if (ui.tab == EditorTab::Route && Hit(kInvertTurns, mouse)) invert = true;
        else if (ui.tab == EditorTab::Route && Hit(kCutTurns, mouse)) cut = true;
        else if (ui.tab == EditorTab::Route && Hit(kUndoEdit, mouse)) {
            action.changedChart = draft.Undo();
            notice(action.changedChart ? "UNDO APPLIED" : "NOTHING TO UNDO",
                   !action.changedChart);
            return action;
        } else if (ui.tab == EditorTab::Route && Hit(kRedoEdit, mouse)) {
            action.changedChart = draft.Redo();
            notice(action.changedChart ? "REDO APPLIED" : "NOTHING TO REDO",
                   !action.changedChart);
            return action;
        } else if (ui.tab == EditorTab::Route && Hit(kHeading, mouse)) {
            draft.RotateHeading(1);
            action.changedChart = true;
        } else if (ui.tab == EditorTab::Field && Hit(kFieldHeading, mouse)) {
            draft.RotateHeading(1);
            action.changedChart = true;
        } else if (ui.tab == EditorTab::Route && Hit(kLeftTurn, mouse)) {
            draft.PutTurn(core::Turn::Left);
            action.changedChart = true;
        } else if (ui.tab == EditorTab::Route && Hit(kRightTurn, mouse)) {
            draft.PutTurn(core::Turn::Right);
            action.changedChart = true;
        } else if (ui.tab == EditorTab::Route && Hit(kRemoveTurn, mouse)) {
            deleteTurns = true;
        } else if (ui.tab == EditorTab::Audio && mouse.y >= 371 &&
                   mouse.y <= 409) {
            for (int i = 0; i < 5; ++i) {
                const Rectangle button{92.0f + i * 155.0f, 371.0f, 148.0f, 37.0f};
                if (Hit(button, mouse)) action.speedIndex = i;
            }
        } else if (mouse.y >= kTimelineY && mouse.y <= kTimelineY + kTimelineHeight &&
                   mouse.x >= kTimelineX &&
                   mouse.x <= kTimelineX + kTimelineWidth) {
            const double anchor = ViewBeat(draft, input.timelineViewBeat);
            const double clicked = anchor + (mouse.x - kPlayheadX) / PixelsPerBeat(ui);
            const int beat = std::clamp(static_cast<int>(std::lround(clicked)),
                                        0, draft.Data().route.lengthBeats - 1);
            if (ui.tab == EditorTab::Route && input.shiftHeld) {
                const int origin = draft.SelectedBeat();
                ui.rangeFirst = std::min(origin, beat);
                ui.rangeLast = std::max(origin, beat);
                ui.draggingTimeline = false;
            } else {
                ui.rangeFirst = ui.rangeLast = -1;
                ui.draggingTimeline = ui.tab == EditorTab::Route && input.mouseHeld;
                ui.dragAnchorBeat = beat;
                ui.dragViewBeat = anchor;
            }
            draft.SelectBeat(beat);
        } else if (ui.tab == EditorTab::Route) {
            const auto& chart = draft.Data().route;
            const EditorGrid grid(chart);
            const float tolerance = std::clamp(grid.step * 0.38f, 7.0f, 19.0f);
            for (int y = 0; y < chart.gridHeight; ++y)
                for (int x = 0; x < chart.gridWidth; ++x) {
                    const auto center = grid.Point(core::Cell{x, y});
                    if (std::abs(mouse.x - center.x) <= tolerance &&
                        std::abs(mouse.y - center.y) <= tolerance) {
                        if (draft.SetStart({x, y})) {
                            draft.SelectBeat(0);
                            action.changedChart = true;
                        }
                        return action;
                    }
                }
        }
    }

    // While dragging, freeze the beat-to-pixel transform so the scroll
    // follows the mouse rather than moving the clicked target out from under it.
    if (ui.draggingTimeline && (input.mouseHeld || input.mouseReleased)) {
        const double point = ui.dragViewBeat +
                             (input.mouse.x - kPlayheadX) / PixelsPerBeat(ui);
        const int beat = std::clamp(static_cast<int>(std::lround(point)),
                                    0, draft.Data().route.lengthBeats - 1);
        if (beat != ui.dragAnchorBeat) {
            ui.rangeFirst = std::min(ui.dragAnchorBeat, beat);
            ui.rangeLast = std::max(ui.dragAnchorBeat, beat);
        } else {
            ui.rangeFirst = ui.rangeLast = -1;
        }
        draft.SelectBeat(beat);
    }
    if (input.mouseReleased || (!input.mouseHeld && !input.mouseClick))
        ui.draggingTimeline = false;

    if (input.zoomOut) ui.timelineZoomIndex = std::max(0, ui.timelineZoomIndex - 1);
    if (input.zoomIn) ui.timelineZoomIndex = std::min(kZoomMax, ui.timelineZoomIndex + 1);
    if (input.previousBeat) { draft.StepSelection(-1); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.nextBeat) { draft.StepSelection(1); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.previousBar) { draft.StepSelection(-4); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.nextBar) { draft.StepSelection(4); ui.rangeFirst = ui.rangeLast = -1; }

    if (ui.tab == EditorTab::Route) {
        if (input.movePrevious && !ui.HasRange()) action.changedChart |= draft.MoveTurn(-1);
        if (input.moveNext && !ui.HasRange()) action.changedChart |= draft.MoveTurn(1);
        if (input.addLeft) { draft.PutTurn(core::Turn::Left); action.changedChart = true; }
        if (input.addRight) { draft.PutTurn(core::Turn::Right); action.changedChart = true; }
        const int first = ui.HasRange() ? ui.rangeFirst : draft.SelectedBeat();
        const int last = ui.HasRange() ? ui.rangeLast : draft.SelectedBeat();
        const bool valid = first >= 0 && last < draft.Data().route.lengthBeats;

        if (copy || cut) {
            ui.clipboard = valid ? draft.CopyTurns(first, last)
                                 : core::ChartEditor::Pattern{};
            const bool any = !ui.clipboard.notes.empty();
            if (cut && any) action.changedChart |= draft.DeleteTurns(first, last);
            notice(any ? (cut ? "TURNS CUT - CTRL+V TO PASTE"
                              : "TURNS COPIED - SELECT DESTINATION BEAT")
                       : "NO TURNS IN SELECTION", !any);
        }
        if (paste) {
            const bool ok = draft.PasteTurns(draft.SelectedBeat(), ui.clipboard);
            action.changedChart |= ok;
            notice(ok ? "PATTERN PASTED AT SELECTED BEAT"
                      : "CANNOT PASTE - EMPTY CLIPBOARD, SAME NOTES OR OUT OF BOUNDS", !ok);
        }
        if (invert) {
            const bool ok = valid && draft.InvertTurns(first, last);
            action.changedChart |= ok;
            notice(ok ? "LEFT / RIGHT REVERSED" : "NO TURNS TO INVERT", !ok);
        }
        if (deleteTurns) {
            const bool ok = valid && draft.DeleteTurns(first, last);
            action.changedChart |= ok;
            notice(ok ? "SELECTED TURNS DELETED" : "NO TURNS TO DELETE", !ok);
        }
        if (input.shiftRangePrevious || input.shiftRangeNext) {
            const int delta = input.shiftRangePrevious ? -1 : 1;
            const bool ok = ui.HasRange() && valid &&
                            draft.ShiftTurns(first, last, delta);
            action.changedChart |= ok;
            if (ok) {
                ui.rangeFirst += delta;
                ui.rangeLast += delta;
                draft.StepSelection(delta);
            }
            notice(ok ? "PATTERN SHIFTED ONE BEAT"
                      : "SHIFT BLOCKED - SELECT TURNS OR AVOID COLLISIONS", !ok);
        }
    }
    if (ui.tab != EditorTab::Audio) {
        if (input.headingLeft) { draft.RotateHeading(-1); action.changedChart = true; }
        if (input.headingRight) { draft.RotateHeading(1); action.changedChart = true; }
    }
    if (input.mouseWheel > 0) draft.StepSelection(-1);
    if (input.mouseWheel < 0) draft.StepSelection(1);
    action.previewToggle |= input.previewToggle;
    action.previewRestart |= input.previewRestart;
    if (input.speedIndex >= 0) action.speedIndex = input.speedIndex;
    return action;
}

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
        Label(font, TextFormat("ROUTE  /  %d x %d FIELD",
                              chart.gridWidth, chart.gridHeight),
              51, 166, 17, Color{188, 215, 227, 255});
        DrawPreviewGrid(font, draft, previewBeat);
        Label(font, "START HEADING  Q/E", 744, 177, 14,
              Color{172, 199, 218, 255});
        Button(font, kHeading, HeadingName(chart.initialHeading),
               Color{119, 229, 188, 255});
        const auto* turn = draft.SelectedTurn();
        Label(font, TextFormat("BEAT %d  /  %s", draft.SelectedBeat(),
                              turn ? (turn->direction == core::Turn::Left
                                      ? "LEFT" : "RIGHT") : "NO TURN"),
              744, 251, 14, Color{216, 232, 239, 255});
        Button(font, kLeftTurn, "F  LEFT", Color{113, 226, 205, 255});
        Button(font, kRightTurn, "J  RIGHT", Color{173, 170, 239, 255});
        Button(font, kRemoveTurn, "DEL  REMOVE", Color{223, 156, 156, 255});
        Label(font, "PATTERN  /  DRAG RULER", 52, 188, 14,
              Color{193, 215, 226, 255});
        Button(font, kCopyTurns, "COPY CTRL+C", Color{126, 190, 221, 255});
        Button(font, kPasteTurns, "PASTE CTRL+V", Color{127, 224, 199, 255});
        Button(font, kInvertTurns, "FLIP L/R  I", Color{190, 168, 229, 255});
        Button(font, kCutTurns, "CUT CTRL+X", Color{216, 166, 155, 255});
        Button(font, kUndoEdit, "UNDO", Color{169, 188, 217, 255});
        Button(font, kRedoEdit, "REDO", Color{169, 188, 217, 255});
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
        Label(font, "PREVIEW SPEED  (1-5 KEYS)", 92, 348, 16,
              Color{182, 207, 227, 255});
        const char* captions[5] = {"100%", "75%", "50%", "25%", "10%"};
        for (int i = 0; i < 5; ++i)
            Button(font, {92.0f + i * 155.0f, 371.0f, 148.0f, 37.0f},
                   captions[i], Color{125, 167, 205, 255},
                   std::abs(playbackSpeed - kSpeeds[i]) < 0.001);
    } else {
        Label(font, "FIELD  /  GAMEPLAY GEOMETRY", 104, 169, 20,
              Color{239, 210, 159, 255});
        Label(font, "NOT A MUSIC TIME SIGNATURE", 548, 172, 14,
              Color{161, 187, 210, 255});
        DrawTextField(font, kGridSize, "FIELD SIZE  W/H  (2..16)",
                      std::to_string(chart.gridWidth) + "/" +
                      std::to_string(chart.gridHeight), EditorField::GridSize, ui);
        DrawTextField(font, kStartCell, "START CELL X/Y  (FROM 0)",
                      std::to_string(chart.start.x) + "/" +
                      std::to_string(chart.start.y), EditorField::StartCell, ui);
        DrawTextField(font, kLength, "TOTAL BEATS",
                      std::to_string(chart.lengthBeats), EditorField::Length, ui);
        Label(font, "START HEADING  Q/E OR CLICK", 519, 299, 16,
              Color{172, 199, 218, 255});
        Button(font, kFieldHeading, HeadingName(chart.initialHeading),
               Color{119, 229, 188, 255});
        Label(font, "EXAMPLE 8/8 = 8 COLUMNS AND 8 ROWS. GRID IS SAVED IN JSON.",
              104, 384, 14, Color{169, 204, 218, 255});
        Label(font, "IF SHRINKING FAILS, MOVE THE START CELL INSIDE THE NEW GRID.",
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
