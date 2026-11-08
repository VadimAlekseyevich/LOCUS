#pragma once

#include "app/chart_editor_ui.h"
#include "app/visual_style.h"
#include "core/route_view.h"

#include <algorithm>
#include <cmath>

namespace locus::app::detail {
constexpr float kVirtualWidth = 960.0f, kVirtualHeight = 540.0f;
constexpr float kGridCenterX = 448.0f, kGridCenterY = 282.0f;
constexpr float kGridMaxSpan = 240.0f;
constexpr float kTimelineX = 50.0f, kTimelineY = 447.0f;
constexpr float kTimelineWidth = 860.0f, kTimelineHeight = 52.0f;
// Fixed playhead at one third of the ruler; the fractional-beat ruler moves.
constexpr float kPlayheadX = kTimelineX + kTimelineWidth / 3.0f;
constexpr float kZoomPixelsPerBeat[7] = {12, 18, 26, 36, 52, 76, 104};
constexpr int kZoomMax = 6;
constexpr float kDragEdgeZone = 48.0f;
// Repeat a four-beat step quickly after a short delay; accelerate on hold.
constexpr float kBarRepeatDelay = 0.23f;
constexpr float kBarRepeatInterval = 0.075f;
constexpr float kBarRepeatFastInterval = 0.04f;
constexpr Rectangle kZoomOut{807, 416, 34, 27};
constexpr Rectangle kZoomIn{863, 416, 34, 27};
constexpr Rectangle kSnapLess{511, 416, 64, 27};
constexpr Rectangle kSnapMore{583, 416, 64, 27};
constexpr int kSnapDivisions[9] = {1, 2, 3, 4, 6, 8, 12, 16, 24};

inline float PixelsPerBeat(const EditorUiState& ui) {
    return kZoomPixelsPerBeat[std::clamp(ui.timelineZoomIndex, 0, kZoomMax)];
}
inline float PixelsPerTick(const EditorUiState& ui, const core::Chart& chart) {
    return PixelsPerBeat(ui) / chart.ticksPerBeat;
}
inline int SnapTick(double rawTick, const core::Chart& chart) {
    const int step = chart.ticksPerBeat / chart.snapDivision;
    const long long snapped = static_cast<long long>(std::llround(rawTick / step)) * step;
    return static_cast<int>(std::clamp(snapped,
        static_cast<long long>(chart.firstBeat),
        static_cast<long long>(chart.lengthBeats - 1)));
}

inline double ViewBeat(const core::ChartEditor& draft, double activeBeat) {
    return std::isfinite(activeBeat)
        ? std::clamp(activeBeat,
                     static_cast<double>(draft.Data().route.firstBeat),
                     static_cast<double>(draft.Data().route.lengthBeats))
        : static_cast<double>(draft.SelectedBeat());
}

constexpr Rectangle kTabSet{277, 73, 110, 35};
constexpr Rectangle kTabRoute{397, 73, 132, 35};
constexpr Rectangle kTabAudio{539, 73, 132, 35};
constexpr Rectangle kTabField{681, 73, 132, 35};
constexpr Rectangle kEditorBack{823, 73, 126, 35}; // Obvious exit from editor to Home.

constexpr Rectangle kSong{49, 115, 162, 37};
constexpr Rectangle kPlay{220, 115, 157, 37};
constexpr Rectangle kStop{386, 115, 115, 37};
constexpr Rectangle kTest{510, 115, 102, 37};

constexpr Rectangle kHeading{744, 201, 168, 40};
constexpr Rectangle kLeftTurn{744, 271, 168, 37};
constexpr Rectangle kRightTurn{744, 314, 168, 37};
constexpr Rectangle kRemoveTurn{744, 357, 168, 37};
// Eight directions around DEL (center); positioned above the timeline.
constexpr Rectangle DirectionPadBounds(int row, int col) noexcept {
    return {744.0f + col * 56.0f, 275.0f + row * 39.0f, 52.0f, 35.0f};
}
inline std::optional<core::Heading> DirectionPadHeading(int row, int col) noexcept {
    constexpr core::Heading values[9] = {
        core::Heading::UpLeft, core::Heading::Up, core::Heading::UpRight,
        core::Heading::Left, core::Heading::Up, core::Heading::Right,
        core::Heading::DownLeft, core::Heading::Down, core::Heading::DownRight
    };
    if (row < 0 || row > 2 || col < 0 || col > 2 ||
        (row == 1 && col == 1)) return std::nullopt;
    return values[row * 3 + col];
}
constexpr Rectangle kCopyTurns{52, 213, 173, 31};
constexpr Rectangle kPasteTurns{52, 248, 173, 31};
constexpr Rectangle kInvertTurns{52, 283, 173, 31};
constexpr Rectangle kCutTurns{52, 318, 173, 31};
constexpr Rectangle kUndoEdit{52, 354, 83, 31};
constexpr Rectangle kRedoEdit{142, 354, 83, 31};

constexpr Rectangle kTitle{92, 205, 768, 43};
constexpr Rectangle kBpm{92, 290, 350, 43};
constexpr Rectangle kOffset{510, 290, 350, 43};
// Song Set management occupies its own tab without moving the timeline.
constexpr Rectangle kSetOpen{52, 304, 123, 33};
constexpr Rectangle kSetNew{184, 304, 123, 33};
constexpr Rectangle kSetDuplicate{316, 304, 132, 33};
constexpr Rectangle kSetImport{52, 345, 123, 33};
constexpr Rectangle kSetExport{184, 345, 123, 33};
constexpr Rectangle kSetDeleteDifficulty{316, 345, 132, 33};
constexpr Rectangle kSetDeleteSong{52, 386, 190, 33};
constexpr Rectangle kSetRestore{251, 386, 197, 33};
constexpr Rectangle kSetPrevious{507, 394, 118, 29};
constexpr Rectangle kSetNext{785, 394, 118, 29};
constexpr Rectangle SetDifficultyRow(int row) noexcept {
    return {503.0f, 202.0f + row * 36.0f, 400.0f, 32.0f};
}
inline EditorSetAction ClickedSetAction(Vector2 p) noexcept {
    if (CheckCollisionPointRec(p, kSetOpen)) return EditorSetAction::Open;
    if (CheckCollisionPointRec(p, kSetNew)) return EditorSetAction::NewDifficulty;
    if (CheckCollisionPointRec(p, kSetDuplicate)) return EditorSetAction::Duplicate;
    if (CheckCollisionPointRec(p, kSetImport)) return EditorSetAction::Import;
    if (CheckCollisionPointRec(p, kSetExport)) return EditorSetAction::Export;
    if (CheckCollisionPointRec(p, kSetDeleteDifficulty)) return EditorSetAction::DeleteDifficulty;
    if (CheckCollisionPointRec(p, kSetDeleteSong)) return EditorSetAction::DeleteSongSet;
    if (CheckCollisionPointRec(p, kSetRestore)) return EditorSetAction::Restore;
    return EditorSetAction::None;
}
constexpr Rectangle kGridSize{104, 225, 330, 47};
constexpr Rectangle kFieldHeading{519, 321, 330, 47};

constexpr double kSpeeds[5] = {1.0, 0.75, 0.5, 0.25, 0.1};

inline Rectangle RouteSpeedBounds(int index) {
    const auto& layout = kDefaultEditorRouteLayout;
    return {layout.practiceSpeedX,
            layout.practiceSpeedStartY + index *
                (layout.practiceSpeedHeight + layout.practiceSpeedGap),
            layout.practiceSpeedWidth, layout.practiceSpeedHeight};
}

inline bool Hit(Rectangle bounds, Vector2 pos) { return CheckCollisionPointRec(pos, bounds); }

// Camera for the uncluttered, continuous ROUTE preview.
// The chart's view size only adjusts magnification, not route geometry.
struct EditorView {
    float left = kGridCenterX, top = kGridCenterY, step = 1.0f;
    core::GridPoint origin{};
    int ticksPerBeat = 1;

    explicit EditorView(const core::Chart& chart, double tick) {
        const int visibleCells = std::max(chart.gridWidth, chart.gridHeight);
        step = kGridMaxSpan / static_cast<float>(std::max(1, visibleCells));
        origin = core::RoutePositionAt(chart, tick);
        ticksPerBeat = chart.ticksPerBeat;
    }
    Vector2 Point(core::GridPoint raw) const {
        return {left + static_cast<float>((raw.x - origin.x) * step / ticksPerBeat),
                top + static_cast<float>((raw.y - origin.y) * step / ticksPerBeat)};
    }
};
inline EditorTab ClickedTab(Vector2 mouse, EditorTab previous) {
    if (Hit(kTabSet, mouse)) return EditorTab::Set;
    if (Hit(kTabRoute, mouse)) return EditorTab::Route;
    if (Hit(kTabAudio, mouse)) return EditorTab::Audio;
    if (Hit(kTabField, mouse)) return EditorTab::Field;
    return previous;
}

 } // namespace locus::app::detail
