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
constexpr float kGridX = 96.0f, kGridY = 197.0f, kGridStep = 59.0f;
constexpr float kTimelineX = 50.0f, kTimelineY = 454.0f;
constexpr float kCellWidth = 53.75f;
constexpr int kVisibleBeats = 16;

constexpr Rectangle kTitle{49, 118, 321, 35};
constexpr Rectangle kBpm{416, 174, 176, 43};
constexpr Rectangle kOffset{630, 174, 244, 43};
constexpr Rectangle kHeading{416, 249, 176, 42};
constexpr Rectangle kLength{630, 249, 244, 42};

constexpr Rectangle kSong{416, 82, 135, 39};
constexpr Rectangle kPlay{559, 82, 143, 39};
constexpr Rectangle kStop{710, 82, 82, 39};
constexpr Rectangle kTest{800, 82, 91, 39};
constexpr Rectangle kLeftTurn{416, 365, 137, 37};
constexpr Rectangle kRightTurn{561, 365, 137, 37};
constexpr Rectangle kRemoveTurn{706, 365, 169, 37};
constexpr double kSpeeds[5] = {1.0, 0.75, 0.5, 0.25, 0.1};

bool Hit(Rectangle bounds, Vector2 pos) { return CheckCollisionPointRec(pos, bounds); }

void Label(Font font, const char* value, float x, float y, float size, Color color) {
    DrawTextEx(font, value, {x, y}, size, 1.0f, color);
}

std::string Truncate(const std::string& value, std::size_t maxCharacters) {
    if (value.size() <= maxCharacters) return value;
    return value.substr(0, maxCharacters - 3) + "...";
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

Vector2 Point(core::GridPoint grid) {
    return {kGridX + static_cast<float>(grid.x) * kGridStep,
            kGridY + static_cast<float>(grid.y) * kGridStep};
}
Vector2 Point(core::Cell cell) {
    return Point(core::GridPoint{static_cast<double>(cell.x),
                                static_cast<double>(cell.y)});
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
        case EditorField::None: break;
    }
    return {0, 0, 0, 0};
}

EditorField ClickedField(Vector2 mouse) {
    for (const auto field : {EditorField::Title, EditorField::Bpm,
                             EditorField::OffsetMs, EditorField::Length}) {
        if (Hit(FieldBounds(field), mouse)) return field;
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
        default: return "";
    }
}

void BeginField(EditorUiState& ui, const core::ChartEditor& draft, EditorField field) {
    ui.active = field;
    ui.buffer = InitialValue(draft, field);
    ui.feedback = "";
}

bool ParseNumeric(const std::string& string, double& result) {
    if (string.empty()) return false;
    char* end = nullptr;
    result = std::strtod(string.c_str(), &end);
    return end && end != string.c_str() && *end == '\0' && std::isfinite(result);
}

bool CommitField(EditorUiState& ui, core::ChartEditor& draft) {
    if (!ui.EditingText()) return true;
    const auto field = ui.active;
    bool valid = false;
    if (field == EditorField::Title) {
        valid = draft.SetTitle(ui.buffer);
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
    Label(font, "BEAT-BY-BEAT ROUTE PREVIEW", 50, 164, 17,
          Color{188, 215, 227, 255});
    for (int line = 0; line < 4; ++line) {
        DrawLineEx(Point(core::Cell{0, line}), Point(core::Cell{3, line}),
                   2, Color{51, 62, 79, 255});
        DrawLineEx(Point(core::Cell{line, 0}), Point(core::Cell{line, 3}),
                   2, Color{51, 62, 79, 255});
    }
    // The marker, cyan travelled line and one-beat blue lookahead use
    // the same route geometry as actual gameplay (including edge Wrap).
    const double bounded = std::clamp(previewBeat, 0.0,
                                      static_cast<double>(chart.lengthBeats));
    const int beat = std::min(static_cast<int>(std::floor(bounded)),
                              chart.lengthBeats - 1);
    const double progress = std::clamp(bounded - beat, 0.0, 1.0);
    const core::RouteStep current = core::SampleRouteStep(chart, beat);
    const core::StepView whole = core::ViewRouteStep(current, progress);
    const core::StepView trace = core::TraceRouteStep(current, progress);

    const auto drawSegments = [&](const core::StepView& view, Color tint, float width) {
        for (int i = 0; i < view.segmentCount; ++i) {
            DrawLineEx(Point(view.segments[i].from), Point(view.segments[i].to), width, tint);
        }
    };

    if (beat + 1 < chart.lengthBeats) {
        const core::RouteStep next = core::SampleRouteStep(chart, beat + 1);
        drawSegments(core::ViewRouteStep(next, 1.0),
                     Color{88, 153, 199, 110}, 3.0f);
    }
    drawSegments(whole, Color{95, 179, 230, 200}, 4.0f);
    drawSegments(trace, Color{110, 235, 218, 255}, 5.5f);

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const Vector2 position = Point(core::Cell{x, y});
            DrawCircleV(position, 8.0f, Color{105, 127, 148, 255});
            DrawCircleV(position, 3.3f, Color{207, 221, 234, 255});
        }
    }
    const Vector2 marker = Point(trace.marker);
    DrawCircleV(marker, 22.0f, Color{112, 219, 216, 48});
    DrawCircleV(marker, 13.0f, Color{124, 229, 225, 255});
    DrawCircleV(marker, 4.0f, Color{246, 251, 252, 255});
    Label(font, TextFormat("BEAT %.2f / %d", bounded, chart.lengthBeats),
          53, 390, 16, Color{158, 225, 212, 255});
}

void DrawTimeline(Font font, const core::ChartEditor& draft) {
    const auto& chart = draft.Data().route;
    const int page = draft.SelectedBeat() / kVisibleBeats * kVisibleBeats;
    Label(font, TextFormat("TIMELINE   %d - %d",
                          page, std::min(page + 15, chart.lengthBeats - 1)),
          50, 422, 17, Color{209, 222, 234, 255});
    Label(font, "CLICK BEAT   /   ARROWS STEP   /   SHIFT+ARROWS MOVE TURN",
          357, 422, 14, Color{164, 188, 208, 255});
    for (int index = 0; index < kVisibleBeats; ++index) {
        const int beat = page + index;
        if (beat >= chart.lengthBeats) continue;
        const Rectangle cell{kTimelineX + index * kCellWidth, kTimelineY,
                             kCellWidth - 3.0f, 46.0f};
        const bool selected = draft.SelectedBeat() == beat;
        DrawRectangleRec(cell, selected ? Color{48, 82, 88, 255}
                                        : Color{38, 47, 61, 255});
        DrawRectangleLinesEx(cell, selected ? 2.0f : 1.0f,
                             selected ? Color{138, 235, 215, 255}
                                      : Color{84, 102, 126, 255});
        Label(font, TextFormat("%d", beat), cell.x + 6, cell.y + 3, 14,
              Color{224, 235, 239, 255});
        const auto it = std::lower_bound(
            chart.turns.begin(), chart.turns.end(), beat,
            [](const core::TurnEvent& event, int n) { return event.beat < n; });
        if (it != chart.turns.end() && it->beat == beat) {
            DrawRectangle(static_cast<int>(cell.x + 5), static_cast<int>(cell.y + 26),
                          static_cast<int>(cell.width - 10), 15,
                          it->direction == core::Turn::Left
                              ? Color{96, 220, 193, 255}
                              : Color{175, 155, 236, 255});
            Label(font, it->direction == core::Turn::Left ? "L" : "R",
                  cell.x + 19, cell.y + 25, 13, Color{22, 26, 36, 255});
        }
    }
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
    input.previousBeat = IsKeyPressed(KEY_LEFT) && !shift;
    input.nextBeat = IsKeyPressed(KEY_RIGHT) && !shift;
    input.movePrevious = IsKeyPressed(KEY_LEFT) && shift;
    input.moveNext = IsKeyPressed(KEY_RIGHT) && shift;
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
    input.mouse = EditorVirtualMouse(GetMousePosition(), GetScreenWidth(),
                                     GetScreenHeight());
    input.mouseWheel = GetMouseWheelMove();
    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        int bytes = 0;
        const char* utf8 = CodepointToUTF8(codepoint, &bytes);
        if (utf8 && bytes > 0) input.typedUtf8.append(utf8, bytes);
        codepoint = GetCharPressed();
    }
    return input;
}

EditorUiAction ApplyEditorControls(core::ChartEditor& draft, EditorUiState& ui,
                                  const EditorControls& input) {
    EditorUiAction action;
    const bool wasDirty = draft.Dirty();
    if (ui.EditingText()) {
        if (input.escape) {
            ui.active = EditorField::None;
            ui.buffer.clear();
            ui.feedback.clear();
        } else if (input.backspace) {
            if (!ui.buffer.empty()) {
                // Drop the entire last UTF-8 codepoint, not just a trailing byte.
                std::size_t pos = ui.buffer.size() - 1;
                while (pos > 0 &&
                       (static_cast<unsigned char>(ui.buffer[pos]) & 0xc0) == 0x80)
                    --pos;
                ui.buffer.erase(pos);
            }
        }
        if (!input.typedUtf8.empty() && ui.buffer.size() < 100) {
            for (unsigned char c : input.typedUtf8) {
                if (ui.active == EditorField::Title ||
                    c == '.' || c == '-' || (c >= '0' && c <= '9')) {
                    if (ui.buffer.size() < 100) ui.buffer.push_back(static_cast<char>(c));
                }
            }
        }
        if (input.enter || input.tab) {
            if (CommitField(ui, draft) && input.tab)
                BeginField(ui, draft, EditorField::Bpm);
        } else if (input.mouseClick) {
            const EditorField target = ClickedField(input.mouse);
            if (target != ui.active) {
                if (CommitField(ui, draft) && target != EditorField::None)
                    BeginField(ui, draft, target);
            }
        }
        action.changedChart = !wasDirty && draft.Dirty();
        return action;
    }

    if (input.mouseClick) {
        const EditorField selected = ClickedField(input.mouse);
        if (selected != EditorField::None) {
            BeginField(ui, draft, selected);
            return action;
        }
        const Vector2 mouse = input.mouse;
        if (Hit(kSong, mouse)) action.chooseAudio = true;
        else if (Hit(kPlay, mouse)) action.previewToggle = true;
        else if (Hit(kStop, mouse)) action.previewRestart = true;
        else if (Hit(kTest, mouse)) action.testPlay = true;
        else if (Hit(kHeading, mouse)) { draft.RotateHeading(1); action.changedChart = true; }
        else if (Hit(kLeftTurn, mouse)) { draft.PutTurn(core::Turn::Left); action.changedChart = true; }
        else if (Hit(kRightTurn, mouse)) { draft.PutTurn(core::Turn::Right); action.changedChart = true; }
        else if (Hit(kRemoveTurn, mouse)) { action.changedChart = draft.RemoveTurn(); }
        else if (mouse.y >= 311 && mouse.y <= 348) {
            for (int i = 0; i < 5; ++i) {
                const Rectangle button{416.0f + i * 93.0f, 312.0f, 86.0f, 35.0f};
                if (Hit(button, mouse)) action.speedIndex = i;
            }
        } else if (mouse.y >= kTimelineY && mouse.y <= kTimelineY + 46 &&
                   mouse.x >= kTimelineX &&
                   mouse.x < kTimelineX + kCellWidth * kVisibleBeats) {
            const int beat = draft.SelectedBeat() / kVisibleBeats * kVisibleBeats +
                             static_cast<int>((mouse.x - kTimelineX) / kCellWidth);
            draft.SelectBeat(beat);
        } else {
            for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
                const auto center = Point(core::Cell{x, y});
                if (std::abs(mouse.x - center.x) < 18 &&
                    std::abs(mouse.y - center.y) < 18) {
                    action.changedChart = draft.SetStart({x, y});
                }
            }
        }
    }

    if (input.previousBeat) draft.SelectBeat(draft.SelectedBeat() - 1);
    if (input.nextBeat) draft.SelectBeat(draft.SelectedBeat() + 1);
    if (input.movePrevious) action.changedChart |= draft.MoveTurn(-1);
    if (input.moveNext) action.changedChart |= draft.MoveTurn(1);
    if (input.addLeft) { draft.PutTurn(core::Turn::Left); action.changedChart = true; }
    if (input.addRight) { draft.PutTurn(core::Turn::Right); action.changedChart = true; }
    if (input.removeTurn) action.changedChart |= draft.RemoveTurn();
    if (input.headingLeft) { draft.RotateHeading(-1); action.changedChart = true; }
    if (input.headingRight) { draft.RotateHeading(1); action.changedChart = true; }
    if (input.mouseWheel > 0) draft.SelectBeat(draft.SelectedBeat() - 1);
    if (input.mouseWheel < 0) draft.SelectBeat(draft.SelectedBeat() + 1);
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
    Label(font, "CHART EDITOR", 49, 75, 31, Color{194, 236, 231, 255});
    Button(font, kSong, "LOAD SONG  O", Color{97, 222, 193, 255});
    Button(font, kPlay, previewPlaying && !previewPaused ? "PAUSE SPACE" : "PLAY SPACE",
           Color{137, 211, 235, 255});
    Button(font, kStop, "RESTART R", Color{211, 178, 137, 255});
    Button(font, kTest, "TEST T", Color{188, 176, 232, 255});
    Label(font, hasAudio ? Truncate(songName, 51).c_str()
                         : "NO AUDIO SELECTED - CLICK LOAD SONG",
          421, 129, 16,
          hasAudio ? Color{178, 227, 222, 255} : Color{242, 181, 164, 255});

    DrawTextField(font, kTitle, "CHART TITLE (CLICK TO TYPE)",
                  draft.Data().title, EditorField::Title, ui);
    char bpm[40]{}, offset[40]{};
    std::snprintf(bpm, sizeof(bpm), "%.3f", chart.bpm);
    std::snprintf(offset, sizeof(offset), "%.3f", draft.Data().offsetSeconds * 1000.0);
    DrawTextField(font, kBpm, "BPM (TYPE)", bpm, EditorField::Bpm, ui);
    DrawTextField(font, kOffset, "OFFSET IN MILLISECONDS (TYPE)",
                  offset, EditorField::OffsetMs, ui);
    Button(font, kHeading, HeadingName(chart.initialHeading),
           Color{119, 229, 188, 255});
    Label(font, "START HEADING (Q/E OR CLICK)", 416, 230, 16,
          Color{172, 199, 218, 255});
    DrawTextField(font, kLength, "TOTAL BEATS (TYPE)",
                  std::to_string(chart.lengthBeats),
                  EditorField::Length, ui);

    Label(font, "PREVIEW SPEED  (1-5 KEYS)", 416, 292, 16,
          Color{182, 207, 227, 255});
    const char* captions[5] = {"100%", "75%", "50%", "25%", "10%"};
    for (int i = 0; i < 5; ++i) {
        Button(font, {416.0f + i * 93.0f, 312.0f, 86.0f, 35.0f},
               captions[i], Color{125, 167, 205, 255},
               std::abs(playbackSpeed - kSpeeds[i]) < 0.001);
    }
    const auto* turn = draft.SelectedTurn();
    Label(font, TextFormat("BEAT %d  %s", draft.SelectedBeat(),
                          turn ? (turn->direction == core::Turn::Left
                                      ? "LEFT TURN" : "RIGHT TURN") : "NO TURN"),
          416, 347, 16, Color{216, 232, 239, 255});
    Button(font, kLeftTurn, "F  LEFT", Color{113, 226, 205, 255});
    Button(font, kRightTurn, "J  RIGHT", Color{173, 170, 239, 255});
    Button(font, kRemoveTurn, "DEL  REMOVE", Color{223, 156, 156, 255});

    DrawPreviewGrid(font, draft, previewBeat);
    DrawTimeline(font, draft);
    const std::string message = ui.feedback.empty()
        ? (ui.EditingText() ? "TYPE VALUE, ENTER CONFIRMS, ESC CANCELS"
                             : "CTRL+S SAVE  CTRL+O OPEN  H HOME  F11 FULLSCREEN")
        : ui.feedback;
    Label(font, Truncate(message, 90).c_str(), 50, 508, 14,
          ui.feedback.empty() ? Color{177, 198, 218, 255}
                              : Color{238, 166, 154, 255});
}

} // namespace locus::app
