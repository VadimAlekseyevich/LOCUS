#include "app/chart_editor_ui.h"
#include "core/utf8.h"
#include "app/chart_editor_layout.h"
#include "core/judgement.h"
#include "app/route_renderer.h"
#include "app/viewport.h"
#include "app/ui_button.h"

#include <algorithm>
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

void DrawRoutePreview(Font font, const core::ChartEditor& draft,
                      double previewBeat, const Skin& skin) {
    const auto& chart = draft.Data().route;
    const double bounded = std::clamp(previewBeat,
                                     static_cast<double>(chart.firstBeat),
                                     static_cast<double>(chart.lengthBeats));
    const EditorView view(chart, bounded);
    const RouteCanvas canvas{view.left, view.top,
                             view.step / chart.ticksPerBeat, view.origin};

    // Grid-free moving preview; the square is a clipping region only.
    // The normalized route and its junctions need not land on grid nodes.
    const float x0 = kGridCenterX - kGridMaxSpan * 0.5f;
    const float y0 = kGridCenterY - kGridMaxSpan * 0.5f;
    const auto clip = ViewportTransform::Menu(
        GetScreenWidth(), GetScreenHeight()).Scissor(
            {x0, y0, kGridMaxSpan, kGridMaxSpan});
    BeginScissorMode(clip.x, clip.y, clip.width, clip.height);

    const Vector2 marker = DrawRouteTrajectory(
        chart, bounded, canvas, RouteRenderMode::Editor, skin);
    DrawRouteJunctions(chart, bounded, canvas, RouteRenderMode::Editor, skin);
    const float markerRadius = std::min(13.0f, view.step * 0.39f);
    DrawCircleV(marker, markerRadius * 1.7f, Color{112, 219, 216, 48});
    DrawCircleV(marker, markerRadius, Color{124, 229, 225, 255});
    DrawCircleV(marker, markerRadius * 0.32f, Color{246, 251, 252, 255});
    EndScissorMode();

    const long long ms = RouteTimeMs(draft, bounded);
    const long long absolute = std::llabs(ms);
    const std::string currentTime = TextFormat("TIME %s%lld:%02lld.%03lld",
        ms < 0 ? "-" : "", absolute / 60000,
        (absolute / 1000) % 60, absolute % 1000);
    Label(font, currentTime.c_str(),
          kRouteTimeValue.x, kRouteTimeValue.y + 3, 15,
          Color{158, 225, 212, 255});
    Label(font, TextFormat("SQUARE VIEW %d", chart.gridWidth),
          266, 405, 15, Color{247, 213, 131, 255});
}

void DrawTimeline(Font font, const core::ChartEditor& draft,
                  const EditorUiState& ui, double previewBeat) {
    const auto& chart = draft.Data().route;
    const double anchor = ui.draggingTimeline
        ? ui.dragViewBeat : ViewBeat(draft, previewBeat);
    const float pixels = PixelsPerTick(ui, chart);
    const int ticks = chart.ticksPerBeat;
    const int snapStep = ticks / chart.snapDivision;

    if (ui.HasRange()) {
        Label(font, TextFormat("RANGE %.3f-%.3f",
                               static_cast<double>(ui.rangeFirst) / ticks,
                               static_cast<double>(ui.rangeLast) / ticks),
              50, 422, 17, Color{239, 207, 131, 255});
    } else {
        Label(font, "TIMELINE", 50, 422, 14,
              Color{209, 222, 234, 255});
    }
    Label(font, TextFormat("GRID 1/%d", chart.snapDivision), 367, 423, 13,
          Color{164, 188, 208, 255});
    Button(font, kSnapLess, "LESS", Color{128, 221, 202, 255});
    Button(font, kSnapMore, "MORE", Color{128, 221, 202, 255});
    Label(font, TextFormat("%d PX/BEAT", static_cast<int>(PixelsPerBeat(ui))),
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
            static_cast<float>((ui.rangeFirst - snapStep * 0.5 - anchor) * pixels);
        const float b = kPlayheadX +
            static_cast<float>((ui.rangeLast + snapStep * 0.5 - anchor) * pixels);
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
    for (int tick = first; tick <= last; ++tick) {
        if (tick % snapStep != 0) continue;
        const float x = kPlayheadX +
                        static_cast<float>((static_cast<double>(tick) - anchor) * pixels);
        if (x < kTimelineX + 5.0f || x > kTimelineX + kTimelineWidth - 5.0f)
            continue;
        const bool bar = tick % (4 * ticks) == 0;
        const bool major = tick % ticks == 0;
        DrawLineEx({x, bar ? 462.0f : major ? 468.0f : 474.0f},
                   {x, 486.0f}, bar ? 2.5f : major ? 1.5f : 1.0f,
                   bar ? Color{199, 220, 234, 255}
                     : major ? Color{113, 141, 163, 255}
                             : Color{81, 126, 135, 220});
    }

    // Draw absolute compass directions: cardinal=teal, diagonal=violet.
    // Iterate authored turns (already sorted), stopping after the view.
    for (const auto& event : chart.turns) {
        if (event.beat < first) continue;
        if (event.beat > last) break;
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
        DrawRectangleRec({x - width * 0.5f, 483.0f, width, 13.0f}, tint);
        if (!narrow) {
            constexpr const char* names[] = {
                "U", "R", "D", "L", "UR", "DR", "DL", "UL"
            };
            const char* label = names[static_cast<int>(event.direction)];
            const float textWidth = MeasureTextEx(font, label, 11, 1).x;
            Label(font, label, x - textWidth * 0.5f, 482.0f, 11,
                  Color{18, 26, 36, 255});
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
                       double previewBeat, double playbackSpeed,
                       const GameSettings& settings, const Skin& skin,
                       const EditorSetView& set) {
    const auto& chart = draft.Data().route;
    Label(font, "CHART EDITOR", 49, 80, 22, Color{194, 236, 231, 255});
    Button(font, kTabSet, "SET", Color{244, 197, 124, 255},
           ui.tab == EditorTab::Set);
    Button(font, kTabRoute, "ROUTE", Color{126, 219, 209, 255},
           ui.tab == EditorTab::Route);
    Button(font, kTabAudio, "AUDIO", Color{131, 190, 237, 255},
           ui.tab == EditorTab::Audio);
    Button(font, kTabField, "FIELD", Color{239, 203, 129, 255},
           ui.tab == EditorTab::Field);
    Button(font, kEditorBack, "BACK", Color{247, 202, 145, 255});

    // Transport and timeline persist across editor tabs.
    // Direction placement is mouse-only, independent of game bindings.
    Button(font, kSong, IsGameDirectionKey(settings, KEY_O)
           ? "LOAD SONG" : "LOAD SONG  O", Color{97, 222, 193, 255});
    const bool running = previewPlaying && !previewPaused;
    Button(font, kPlay, IsGameDirectionKey(settings, KEY_SPACE)
           ? (running ? "PAUSE" : "PLAY")
           : (running ? "PAUSE SPACE" : "PLAY SPACE"),
           Color{137, 211, 235, 255});
    Button(font, kStop, IsGameDirectionKey(settings, KEY_R)
           ? "RESTART" : "RESTART R", Color{211, 178, 137, 255});
    Button(font, kTest, IsGameDirectionKey(settings, KEY_T)
           ? "TEST" : "TEST T", Color{188, 176, 232, 255});
    Label(font, hasAudio ? Truncate(songName, 30).c_str()
                         : "NO SONG - LOAD SONG",
          626, 127, 15,
          hasAudio ? Color{178, 227, 222, 255} : Color{242, 181, 164, 255});

    if (ui.tab == EditorTab::Route) {
        Label(font, TextFormat("ROUTE / %d x %d   DIFFICULTY V2  %.1f",
                              chart.gridWidth, chart.gridHeight,
                              draft.Difficulty().overall),
              51, 166, 16, Color{188, 215, 227, 255});
        DrawRoutePreview(font, draft, previewBeat, skin);
        Label(font, IsGameDirectionKey(settings, KEY_Q) ||
                    IsGameDirectionKey(settings, KEY_E)
               ? "START HEADING (CLICK)" : "START HEADING Q/E 45",
              744, 177, 13,
              Color{172, 199, 218, 255});
        Button(font, kHeading, HeadingName(chart.initialHeading),
               Color{119, 229, 188, 255});
        const auto* turn = draft.SelectedTurn();
        const auto incoming = core::IncomingHeadingAt(chart, draft.SelectedBeat());
        const auto outgoing = turn ? turn->direction : incoming;
        Label(font, TextFormat("BEAT %.3f / %s",
                              static_cast<double>(draft.SelectedBeat()) /
                                  chart.ticksPerBeat,
                              turn ? core::DirectionName(turn->direction) : "STRAIGHT"),
              744, 250, 12, Color{216, 232, 239, 255});
        // Full-chart totals, including each diagonal's two cardinals.
        const auto pressCounts = DirectionPressCounts(chart);
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
        Label(font, "CLICK TO PLACE / DEL TO REMOVE", 744, 398, 11,
              Color{165, 198, 213, 255});
        Label(font, "PATTERN  /  DRAG RULER", 52, 188, 14,
              Color{193, 215, 226, 255});
        Button(font, kCopyTurns, "COPY CTRL+C", Color{126, 190, 221, 255});
        Button(font, kPasteTurns, "PASTE CTRL+V", Color{127, 224, 199, 255});
        Button(font, kMirrorHorizontal, "FLIP H", Color{190, 168, 229, 255});
        Button(font, kMirrorVertical, "FLIP V", Color{190, 168, 229, 255});
        Button(font, kRotateCounterclockwise, "ROT -90", Color{140, 196, 237, 255});
        Button(font, kRotateClockwise, "ROT +90", Color{140, 196, 237, 255});
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
        DrawTextField(font, kTitle, "DIFFICULTY NAME (TYPE)", draft.Data().title,
                      EditorField::Title, ui);
        const auto& rating = draft.Difficulty();
        Label(font, TextFormat("RATING V2 %.1f  SPEED %.1f  RHYTHM %.1f  KEYS %.1f  READ %.1f  END %.1f",
              rating.overall, rating.speed, rating.rhythm,
              rating.coordination, rating.reading, rating.stamina),
              92, 252, 12, Color{239, 202, 137, 255});
        char bpm[40]{}, offset[40]{};
        std::snprintf(bpm, sizeof(bpm), "%.3f", chart.bpm);
        std::snprintf(offset, sizeof(offset), "%.0f",
                      draft.Data().offsetSeconds * 1000.0);
        DrawTextField(font, kBpm, "SONG BPM (SHARED)", bpm, EditorField::Bpm, ui);
        DrawTextField(font, kOffset, "SONG OFFSET MS (SHARED)",
                      offset, EditorField::OffsetMs, ui);
        Label(font, "SONG PREVIEW START (MS) - SHARED BY ALL DIFFICULTIES",
              92, 348, 16, Color{172, 199, 218, 255});
        DrawRectangleRounded(kAudioPreviewMs, 0.14f, 5,
                             ui.editingSetPreview ? Color{47, 68, 78, 255}
                                                  : Color{35, 43, 56, 255});
        DrawRectangleRoundedLinesEx(kAudioPreviewMs, 0.14f, 5,
                                    ui.editingSetPreview ? 2.0f : 1.0f,
                                    ui.editingSetPreview ? Color{129, 235, 214, 255}
                                                         : Color{91, 114, 139, 255});
        const std::string value = ui.editingSetPreview
            ? ui.setPreviewBuffer
            : (set.managed ? std::to_string(set.previewStartMs)
                           : "SAVE SONG FIRST (CTRL+S)");
        Label(font, value.c_str(), 104, 381, 19,
              set.managed ? Color{225, 235, 241, 255}
                          : Color{159, 181, 201, 255});
        if (ui.editingSetPreview) {
            const float width = MeasureTextEx(font, value.c_str(), 19, 1).x;
            const float x = std::min(844.0f, 105.0f + width);
            DrawLineEx({x, 380}, {x, 404}, 1.6f,
                       Color{186, 251, 235, 255});
        }
    } else if (ui.tab == EditorTab::Set) {
        const Color heading{186, 219, 231, 255};
        const Color ordinary{211, 226, 236, 255};
        const Color amber{239, 202, 137, 255};
        Label(font, "SONG SET / LIBRARY", 52, 170, 20, heading);
        if (set.managed) {
            Label(font, Truncate(set.title, 35).c_str(), 52, 201, 20, ordinary);
            Label(font, Truncate(set.artist.empty() ? "ARTIST: NOT SET"
                : "ARTIST: " + set.artist, 44).c_str(), 52, 233, 14, heading);
            Label(font, Truncate("AUDIO: " + set.audioName, 44).c_str(),
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
        Button(font, kSetOpen, "OPEN", Color{126, 219, 209, 255});
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
        Button(font, kSetPrevious, "PREV ^", Color{120, 176, 207, 255});
        Button(font, kSetNext, "NEXT v", Color{120, 176, 207, 255});
        Label(font, TextFormat("%d-%d / %d",
              set.difficulties.empty() ? 0 : set.firstVisible + 1,
              std::min(set.firstVisible + 5,
                       static_cast<int>(set.difficulties.size())),
              static_cast<int>(set.difficulties.size())),
              641, 400, 14, heading);
    } else {
        Label(font, "FIELD  /  EDITOR GRID VIEW", 104, 169, 20,
              Color{239, 210, 159, 255});
        Label(font, "NOT A MUSIC TIME SIGNATURE", 548, 172, 14,
              Color{161, 187, 210, 255});
        DrawTextField(font, kGridSize, "VISIBLE SQUARE SIDE (2..16)",
                      std::to_string(chart.gridWidth), EditorField::GridSize, ui);
        Label(font, "ONE NUMBER = EQUAL WIDTH / HEIGHT", 505, 231, 14,
              Color{169, 204, 218, 255});
        Label(font, "AUTOMATIC ROUTE ORIGIN", 505, 255, 13,
              Color{169, 204, 218, 255});
        Label(font, draft.HasSongDuration()
              ? TextFormat("SONG TIMELINE: %d TO %d (AUTO FROM BPM / OFFSET)",
                           chart.firstBeat / chart.ticksPerBeat,
                           (chart.lengthBeats - 1) / chart.ticksPerBeat)
              : "SONG TIMELINE: LOAD AUDIO TO CALCULATE",
              104, 329, 15, Color{188, 218, 228, 255});
        Label(font, IsGameDirectionKey(settings, KEY_Q) ||
                    IsGameDirectionKey(settings, KEY_E)
               ? "START HEADING (CLICK)" : "START HEADING  Q/E 45 DEG",
              519, 299, 16,
              Color{172, 199, 218, 255});
        Button(font, kFieldHeading, HeadingName(chart.initialHeading),
               Color{119, 229, 188, 255});
        Label(font, "SONG TIME IS INDEPENDENT OF VISIBLE SQUARE SIZE.",
              104, 384, 14, Color{169, 204, 218, 255});
        Label(font, "PLAY CLIPS THE ROUTE TO ITS CENTRED SQUARE FRAME.",
              104, 403, 13, Color{226, 197, 146, 255});
    }

    DrawTimeline(font, draft, ui, previewBeat);
    const bool horizontalArrows = !IsGameDirectionKey(settings, KEY_LEFT) &&
                                 !IsGameDirectionKey(settings, KEY_RIGHT);
    const bool verticalArrows = !IsGameDirectionKey(settings, KEY_UP) &&
                               !IsGameDirectionKey(settings, KEY_DOWN);
    const std::string navigationHint = horizontalArrows
        ? (verticalArrows ? "LR NAV / UP-DOWN GRID" : "LR NAV / GRID +/-")
        : (IsGameDirectionKey(settings, KEY_PAGE_UP) ||
           IsGameDirectionKey(settings, KEY_PAGE_DOWN)
            ? "WHEEL NAV" : "PGUP/PGDN NAV");
    const std::string message = ui.feedback.empty()
        ? (ui.EditingText() ? "TYPE VALUE, ENTER CONFIRMS, ESC CANCELS"
           : navigationHint + "  ESC BACK  CTRL+S SAVE  CTRL+O OPEN" +
             (IsGameDirectionKey(settings, KEY_H) ? "" : "  H HOME") +
             "  F11 FULLSCREEN")
        : ui.feedback;
    Label(font, Truncate(message, 90).c_str(), 50, 508, 14,
          ui.feedback.empty() ? Color{177, 198, 218, 255}
              : ui.feedbackError ? Color{238, 166, 154, 255}
                                 : Color{142, 232, 207, 255});
}

} // namespace locus::app
