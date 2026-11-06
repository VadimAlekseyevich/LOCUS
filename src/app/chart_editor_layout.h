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

// The ROUTE editor keeps a scrolling lattice in an otherwise infinite world.
// Its width/height determine grid density, not boundaries or Wrap behaviour.
struct EditorGrid {
    float left = kGridCenterX, top = kGridCenterY, step = 1.0f;
    core::GridPoint origin{}; // Raw tick-model coordinates.
    core::Cell start{};
    int ticksPerBeat = 1;

    explicit EditorGrid(const core::Chart& chart, double tick) {
        const int span = std::max(chart.gridWidth - 1, chart.gridHeight - 1);
        step = kGridMaxSpan / static_cast<float>(std::max(1, span));
        origin = core::RoutePositionAt(chart, tick);
        start = chart.start;
        ticksPerBeat = chart.ticksPerBeat;
    }
    // A visual cell remains one musical beat long in any snap resolution.
    Vector2 Point(core::GridPoint raw) const {
        return {left + static_cast<float>((raw.x - origin.x) * step / ticksPerBeat),
                top + static_cast<float>((raw.y - origin.y) * step / ticksPerBeat)};
    }
    Vector2 Point(core::Cell cell) const {
        return Point(core::GridPoint{
            static_cast<double>(start.x + (cell.x - start.x) * ticksPerBeat),
            static_cast<double>(start.y + (cell.y - start.y) * ticksPerBeat)});
    }
};

inline EditorTab ClickedTab(Vector2 mouse, EditorTab previous) {
    if (Hit(kTabRoute, mouse)) return EditorTab::Route;
    if (Hit(kTabAudio, mouse)) return EditorTab::Audio;
    if (Hit(kTabField, mouse)) return EditorTab::Field;
    return previous;
}

 } // namespace locus::app::detail
