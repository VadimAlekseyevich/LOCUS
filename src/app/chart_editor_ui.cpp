#include "app/chart_editor_ui.h"

#include "core/route.h"
#include "core/route_view.h"

#include <algorithm>
#include <cmath>

namespace locus::app {
namespace {
constexpr float kVirtualWidth = 960.0f;
constexpr float kVirtualHeight = 540.0f;
constexpr float kGridX = 96.0f;
constexpr float kGridY = 197.0f;
constexpr float kGridStep = 59.0f;
constexpr float kTimelineX = 50.0f;
constexpr float kTimelineY = 454.0f;
constexpr float kCellWidth = 53.75f;
constexpr int kVisibleBeats = 16;

constexpr Rectangle kBpmMinus{521, 163, 36, 34};
constexpr Rectangle kBpmPlus{567, 163, 36, 34};
constexpr Rectangle kOffsetMinus{796, 163, 36, 34};
constexpr Rectangle kOffsetPlus{846, 163, 36, 34};
constexpr Rectangle kHeadingMinus{523, 254, 36, 34};
constexpr Rectangle kHeadingPlus{574, 254, 36, 34};
constexpr Rectangle kLengthMinus{796, 254, 36, 34};
constexpr Rectangle kLengthPlus{846, 254, 36, 34};
constexpr Rectangle kLeftTurn{415, 353, 100, 37};
constexpr Rectangle kRightTurn{526, 353, 100, 37};
constexpr Rectangle kRemoveTurn{639, 353, 126, 37};

bool Hit(Rectangle rect, Vector2 mouse) noexcept {
    return CheckCollisionPointRec(mouse, rect);
}

void Label(Font font, const char* value, float x, float y, float size, Color color) {
    DrawTextEx(font, value, {x, y}, size, 1.0f, color);
}

void Button(Font font, Rectangle rectangle, const char* caption, Color border) {
    DrawRectangleRounded(rectangle, 0.16f, 5, Color{37, 45, 61, 255});
    DrawRectangleRoundedLinesEx(rectangle, 0.16f, 5, 1.5f, border);
    const auto dimensions = MeasureTextEx(font, caption, 18.0f, 1.0f);
    Label(font, caption, rectangle.x + (rectangle.width - dimensions.x) / 2.0f,
          rectangle.y + 6.0f, 18.0f, Color{223, 229, 241, 255});
}

Vector2 Point(core::GridPoint grid) {
    return {kGridX + static_cast<float>(grid.x) * kGridStep,
            kGridY + static_cast<float>(grid.y) * kGridStep};
}

Vector2 Point(core::Cell cell) {
    return Point(core::GridPoint{static_cast<double>(cell.x),
                                static_cast<double>(cell.y)});
}

const char* Direction(core::Heading heading) {
    switch (heading) {
        case core::Heading::Up: return "UP";
        case core::Heading::Right: return "RIGHT";
        case core::Heading::Down: return "DOWN";
        case core::Heading::Left: return "LEFT";
    }
    return "?";
}

void DrawGrid(Font font, const core::ChartEditor& draft) {
    const auto& chart = draft.Data().route;
    Label(font, "START CELL  -  CLICK A DOT", 49, 147, 18, Color{195, 209, 230, 255});
    constexpr Color guides{59, 66, 83, 255};
    for (int index = 0; index < 4; ++index) {
        DrawLineEx(Point(core::Cell{0, index}), Point(core::Cell{3, index}), 2.0f, guides);
        DrawLineEx(Point(core::Cell{index, 0}), Point(core::Cell{index, 3}), 2.0f, guides);
    }

    // Preview four steps from the selected beat, using the actual authored
    // turn-before-step route. Wrap lines stay disconnected at board seams.
    for (int ahead = 3; ahead >= 0; --ahead) {
        const int beat = draft.SelectedBeat() + ahead;
        if (beat >= chart.lengthBeats) continue;
        const core::RouteStep step = core::SampleRouteStep(chart, beat);
        const core::StepView shape = core::ViewRouteStep(step, 1.0);
        const Color routeColor = ahead == 0 ? Color{109, 230, 219, 255}
                                 : Color{97, 163, 210,
                                         static_cast<unsigned char>(185 - ahead * 31)};
        for (int segment = 0; segment < shape.segmentCount; ++segment) {
            DrawLineEx(Point(shape.segments[segment].from),
                       Point(shape.segments[segment].to),
                       ahead == 0 ? 4.0f : 2.5f, routeColor);
        }
    }

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const Vector2 point = Point(core::Cell{x, y});
            DrawCircleV(point, 9.0f, Color{87, 108, 132, 255});
            DrawCircleV(point, 4.0f, Color{201, 214, 231, 255});
        }
    }
    const Vector2 start = Point(chart.start);
    DrawCircleV(start, 19.0f, Color{117, 230, 186, 56});
    DrawCircleLines(static_cast<int>(start.x), static_cast<int>(start.y), 16.0f,
                    Color{130, 246, 189, 255});
    DrawCircleV(start, 8.0f, Color{139, 244, 194, 255});
    Label(font, TextFormat("START (%d,%d)", chart.start.x, chart.start.y),
          51, 391, 17, Color{142, 233, 184, 255});
}

void DrawTimeline(Font font, const core::ChartEditor& draft) {
    const auto& chart = draft.Data().route;
    const int pageStart = draft.SelectedBeat() / kVisibleBeats * kVisibleBeats;
    Label(font, TextFormat("TIMELINE  /  BEATS %d-%d",
                          pageStart, std::min(pageStart + 15, chart.lengthBeats - 1)),
          50, 420, 17, Color{213, 226, 233, 255});
    Label(font, "ARROWS SELECT  |  SHIFT+ARROWS MOVE A TURN",
          467, 421, 14, Color{165, 187, 205, 255});
    for (int index = 0; index < kVisibleBeats; ++index) {
        const int beat = pageStart + index;
        const Rectangle cell{kTimelineX + index * kCellWidth, kTimelineY,
                             kCellWidth - 3.0f, 46.0f};
        if (beat >= chart.lengthBeats) continue;
        const bool selected = beat == draft.SelectedBeat();
        DrawRectangleRec(cell, selected ? Color{44, 78, 89, 255}
                                        : Color{39, 46, 61, 255});
        DrawRectangleLinesEx(cell, selected ? 2.0f : 1.0f,
                             selected ? Color{132, 228, 215, 255}
                                      : Color{79, 90, 110, 255});
        Label(font, TextFormat("%d", beat), cell.x + 6.0f, cell.y + 3.0f,
              15.0f, Color{220, 226, 234, 255});
        const auto it = std::lower_bound(
            chart.turns.begin(), chart.turns.end(), beat,
            [](const core::TurnEvent& event, int n) { return event.beat < n; });
        if (it != chart.turns.end() && it->beat == beat) {
            const bool left = it->direction == core::Turn::Left;
            DrawRectangle(static_cast<int>(cell.x + 5.0f),
                          static_cast<int>(cell.y + 26.0f),
                          static_cast<int>(cell.width - 10.0f), 15,
                          left ? Color{93, 204, 187, 255}
                               : Color{158, 155, 232, 255});
            Label(font, left ? "L" : "R", cell.x + 19.0f, cell.y + 25.0f,
                  13.0f, Color{19, 27, 39, 255});
        }
    }
}

} // namespace

Vector2 EditorVirtualMouse(Vector2 physical, int screenWidth, int screenHeight) {
    const float scale = std::min(static_cast<float>(screenWidth) / kVirtualWidth,
                                 static_cast<float>(screenHeight) / kVirtualHeight);
    if (scale <= 0.0f) return {-1.0f, -1.0f};
    const float left = (screenWidth - kVirtualWidth * scale) / 2.0f;
    const float top = (screenHeight - kVirtualHeight * scale) / 2.0f;
    return {(physical.x - left) / scale, (physical.y - top) / scale};
}

EditorControls PollEditorControls() {
    EditorControls controls;
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    controls.previousBeat = IsKeyPressed(KEY_LEFT) && !shift;
    controls.nextBeat = IsKeyPressed(KEY_RIGHT) && !shift;
    controls.movePrevious = IsKeyPressed(KEY_LEFT) && shift;
    controls.moveNext = IsKeyPressed(KEY_RIGHT) && shift;
    controls.addLeft = IsKeyPressed(KEY_F);
    controls.addRight = IsKeyPressed(KEY_J);
    controls.removeTurn = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE);
    controls.bpmDown = IsKeyPressed(KEY_Z);
    controls.bpmUp = IsKeyPressed(KEY_X);
    controls.offsetDown = IsKeyPressed(KEY_N);
    controls.offsetUp = IsKeyPressed(KEY_M);
    controls.fineOffset = shift;
    controls.headingLeft = IsKeyPressed(KEY_Q);
    controls.headingRight = IsKeyPressed(KEY_E);
    controls.lengthDown = IsKeyPressed(KEY_LEFT_BRACKET);
    controls.lengthUp = IsKeyPressed(KEY_RIGHT_BRACKET);
    controls.mouseClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    controls.mouse = EditorVirtualMouse(GetMousePosition(), GetScreenWidth(),
                                       GetScreenHeight());
    controls.mouseWheel = GetMouseWheelMove();
    return controls;
}

void ApplyEditorControls(core::ChartEditor& draft, const EditorControls& input) {
    if (input.previousBeat) draft.SelectBeat(draft.SelectedBeat() - 1);
    if (input.nextBeat) draft.SelectBeat(draft.SelectedBeat() + 1);
    if (input.movePrevious) draft.MoveTurn(-1);
    if (input.moveNext) draft.MoveTurn(+1);
    if (input.addLeft) draft.PutTurn(core::Turn::Left);
    if (input.addRight) draft.PutTurn(core::Turn::Right);
    if (input.removeTurn) draft.RemoveTurn();
    if (input.bpmDown) draft.ChangeBpm(-1);
    if (input.bpmUp) draft.ChangeBpm(1);
    if (input.offsetDown) draft.ChangeOffsetMs(input.fineOffset ? -1 : -10);
    if (input.offsetUp) draft.ChangeOffsetMs(input.fineOffset ? 1 : 10);
    if (input.headingLeft) draft.RotateHeading(-1);
    if (input.headingRight) draft.RotateHeading(1);
    if (input.lengthDown) draft.ChangeLength(-16);
    if (input.lengthUp) draft.ChangeLength(16);
    if (input.mouseWheel > 0.0f) draft.SelectBeat(draft.SelectedBeat() - 1);
    if (input.mouseWheel < 0.0f) draft.SelectBeat(draft.SelectedBeat() + 1);

    if (!input.mouseClick) return;
    const Vector2 mouse = input.mouse;
    if (Hit(kBpmMinus, mouse)) draft.ChangeBpm(-1);
    else if (Hit(kBpmPlus, mouse)) draft.ChangeBpm(1);
    else if (Hit(kOffsetMinus, mouse)) draft.ChangeOffsetMs(-10);
    else if (Hit(kOffsetPlus, mouse)) draft.ChangeOffsetMs(10);
    else if (Hit(kHeadingMinus, mouse)) draft.RotateHeading(-1);
    else if (Hit(kHeadingPlus, mouse)) draft.RotateHeading(1);
    else if (Hit(kLengthMinus, mouse)) draft.ChangeLength(-16);
    else if (Hit(kLengthPlus, mouse)) draft.ChangeLength(16);
    else if (Hit(kLeftTurn, mouse)) draft.PutTurn(core::Turn::Left);
    else if (Hit(kRightTurn, mouse)) draft.PutTurn(core::Turn::Right);
    else if (Hit(kRemoveTurn, mouse)) draft.RemoveTurn();
    else if (mouse.y >= kTimelineY && mouse.y <= kTimelineY + 46 &&
             mouse.x >= kTimelineX && mouse.x < kTimelineX + kCellWidth * kVisibleBeats) {
        const int beat = draft.SelectedBeat() / kVisibleBeats * kVisibleBeats +
                         static_cast<int>((mouse.x - kTimelineX) / kCellWidth);
        draft.SelectBeat(beat);
    } else {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                const Vector2 point = Point(core::Cell{x, y});
                if (std::abs(mouse.x - point.x) <= 18.0f &&
                    std::abs(mouse.y - point.y) <= 18.0f) {
                    draft.SetStart({x, y});
                    return;
                }
            }
        }
    }
}

void RenderChartEditor(Font font, const core::ChartEditor& draft) {
    const auto& chart = draft.Data().route;
    Label(font, "CHART EDITOR", 48, 90, 31, Color{194, 237, 229, 255});
    Label(font, "IN-MEMORY DRAFT  |  SAVE ARRIVES IN M-04", 415, 103,
          17, Color{208, 190, 150, 255});
    DrawGrid(font, draft);

    Label(font, "BPM", 416, 139, 17, Color{177, 195, 219, 255});
    Label(font, TextFormat("%.2f", chart.bpm), 413, 166, 24,
          Color{228, 237, 245, 255});
    Button(font, kBpmMinus, "-", Color{108, 151, 174, 255});
    Button(font, kBpmPlus, "+", Color{108, 151, 174, 255});

    Label(font, "OFFSET  /  MILLISECONDS", 649, 139, 17,
          Color{177, 195, 219, 255});
    Label(font, TextFormat("%+.0f ms", draft.Data().offsetSeconds * 1000.0),
          649, 166, 23, Color{231, 236, 242, 255});
    Button(font, kOffsetMinus, "-", Color{108, 151, 174, 255});
    Button(font, kOffsetPlus, "+", Color{108, 151, 174, 255});

    Label(font, "START HEADING", 411, 229, 17, Color{177, 195, 219, 255});
    Label(font, Direction(chart.initialHeading), 413, 260, 20,
          Color{141, 228, 188, 255});
    Button(font, kHeadingMinus, "<", Color{108, 151, 174, 255});
    Button(font, kHeadingPlus, ">", Color{108, 151, 174, 255});
    Label(font, "TOTAL BEATS", 657, 229, 17, Color{177, 195, 219, 255});
    Label(font, TextFormat("%d", chart.lengthBeats), 656, 260, 24,
          Color{231, 236, 242, 255});
    Button(font, kLengthMinus, "-", Color{108, 151, 174, 255});
    Button(font, kLengthPlus, "+", Color{108, 151, 174, 255});

    const core::TurnEvent* turn = draft.SelectedTurn();
    Label(font, TextFormat("SELECTED BEAT  %d  /  %s",
                          draft.SelectedBeat(),
                          turn ? (turn->direction == core::Turn::Left ? "LEFT" : "RIGHT")
                               : "EMPTY"),
          415, 315, 20, Color{220, 230, 240, 255});
    Button(font, kLeftTurn, "F  LEFT", Color{99, 216, 191, 255});
    Button(font, kRightTurn, "J  RIGHT", Color{150, 158, 236, 255});
    Button(font, kRemoveTurn, "DEL  ERASE", Color{223, 152, 151, 255});
    DrawTimeline(font, draft);
    Label(font, "Z/X BPM   N/M +/-10 ms (SHIFT = 1 ms)   Q/E HEADING   [/] BEATS   H HOME",
          50, 507, 14, Color{194, 196, 214, 255});
}

} // namespace locus::app
