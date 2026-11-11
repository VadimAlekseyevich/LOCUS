#pragma once

#include "app/chart_editor_ui.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace locus::app::detail {
constexpr float kVirtualWidth = 960.0f, kVirtualHeight = 540.0f;
constexpr float kGridCenterX = 448.0f, kGridCenterY = 282.0f;
constexpr float kGridMaxSpan = 240.0f;
// Zoomable, note-bearing ruler sits beneath the editor tabs.
// The separate full-song overview is always mapped to actual audio seconds.
constexpr float kTimelineX = 50.0f, kTimelineY = 115.0f;
constexpr float kTimelineWidth = 860.0f, kTimelineHeight = 43.0f;
constexpr Rectangle kSongOverview{52.0f, 466.0f, 856.0f, 35.0f};
// Fixed playhead at one third of the ruler; the fractional-beat ruler moves.
constexpr float kPlayheadX = kTimelineX + kTimelineWidth / 3.0f;
constexpr float kZoomPixelsPerBeat[7] = {12, 18, 26, 36, 52, 76, 104};
constexpr int kZoomMax = 6;
constexpr float kDragEdgeZone = 48.0f;
// Repeat a four-beat step quickly after a short delay; accelerate on hold.
constexpr float kBarRepeatDelay = 0.23f;
constexpr float kBarRepeatInterval = 0.075f;
constexpr float kBarRepeatFastInterval = 0.04f;
constexpr Rectangle kZoomOut{744, 202, 78, 30};
constexpr Rectangle kZoomIn{830, 202, 78, 30};
constexpr Rectangle kSnapLess{744, 166, 78, 30};
constexpr Rectangle kSnapMore{830, 166, 78, 30};
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

// Non-ROUTE transport sits below the overview rather than obscuring the ruler.
constexpr Rectangle kSong{52, 505, 103, 28};
constexpr Rectangle kPlay{164, 505, 98, 28};
constexpr Rectangle kStop{271, 505, 111, 28};
constexpr Rectangle kTest{391, 505, 103, 28};
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
// Live counts of physical direction presses in the full chart.
// A diagonal increments its own pad plus both cardinal components.
inline std::array<int, 8> DirectionPressCounts(const core::Chart& chart) noexcept {
    std::array<int, 8> counts{};
    for (const auto& note : chart.turns) {
        if (!core::ValidHeading(note.direction)) continue;
        const unsigned mask = core::HeadingArrowMask(note.direction);
        for (int cardinal = 0; cardinal < 4; ++cardinal)
            if (mask & (1u << cardinal)) ++counts[static_cast<std::size_t>(cardinal)];
        if (core::IsDiagonal(note.direction))
            ++counts[static_cast<std::size_t>(note.direction)];
    }
    return counts;
}

// Same click geometry is used by the pure controls and renderer.
constexpr Rectangle kRouteTest{52, 166, 120, 30};
constexpr Rectangle kCopyTurns{184, 166, 100, 30};
constexpr Rectangle kPasteTurns{294, 166, 100, 30};
constexpr Rectangle kCutTurns{404, 166, 92, 30};
constexpr Rectangle kUndoEdit{506, 166, 108, 30};
constexpr Rectangle kRedoEdit{624, 166, 110, 30};
constexpr Rectangle kMirrorHorizontal{52, 202, 127, 30};
constexpr Rectangle kMirrorVertical{190, 202, 127, 30};
constexpr Rectangle kRotateCounterclockwise{328, 202, 127, 30};
constexpr Rectangle kRotateClockwise{466, 202, 127, 30};
constexpr Rectangle kCompareToggle{604, 202, 130, 30};
constexpr Rectangle kComparePopup{489, 240, 245, 207};
constexpr Rectangle ComparePickerRow(int row) noexcept {
    return {499.0f, 264.0f + row * 29.0f, 225.0f, 25.0f};
}
constexpr Rectangle kComparePickerPrev{503, 420, 80, 20};
constexpr Rectangle kComparePickerNext{638, 420, 84, 20};

// All preview panes share one physical song-time scale. Primary stays large
// even when there are four routes; references shrink instead of the editor.
constexpr float kPreviewLeft = 52.0f;
constexpr float kPreviewRight = 734.0f;
constexpr float kPreviewTop = 239.0f;
constexpr float kPreviewSide = 198.0f;
constexpr float kPreviewGap = 8.0f;
inline Rectangle ComparisonPreviewBounds(int count, int index) noexcept {
    const int visible = std::clamp(count, 1, 4);
    const int followers = visible - 1;
    const float secondary = followers == 0 ? 0.0f :
        std::min(kPreviewSide,
            (kPreviewRight - kPreviewLeft - kPreviewSide -
             kPreviewGap * followers) / followers);
    const float total = kPreviewSide + followers * (secondary + kPreviewGap);
    const float x0 = kPreviewLeft +
        (kPreviewRight - kPreviewLeft - total) * 0.5f;
    if (index <= 0) return {x0, kPreviewTop, kPreviewSide, kPreviewSide};
    if (index >= visible) return {};
    return {x0 + kPreviewSide + kPreviewGap +
        (index - 1) * (secondary + kPreviewGap),
        kPreviewTop + (kPreviewSide - secondary) * 0.5f,
        secondary, secondary};
}

constexpr Rectangle kTitle{92, 205, 768, 43};
constexpr Rectangle kBpm{92, 290, 350, 43};
constexpr Rectangle kOffset{510, 290, 350, 43};
constexpr Rectangle kAudioPreviewMs{92, 371, 768, 43};
// Song Set management occupies its own tab without moving the timeline.
constexpr Rectangle kSetOpenFolder{251, 270, 197, 27};
constexpr Rectangle kSetBackground{52, 301, 190, 31};
constexpr Rectangle kSetRemoveBackground{251, 301, 197, 31};
constexpr Rectangle kSetNew{52, 340, 190, 27};
constexpr Rectangle kSetDuplicate{251, 340, 197, 27};
constexpr Rectangle kSetImport{52, 371, 123, 27};
constexpr Rectangle kSetExport{184, 371, 123, 27};
constexpr Rectangle kSetDeleteDifficulty{316, 371, 132, 27};
constexpr Rectangle kSetDeleteSong{52, 402, 190, 27};
constexpr Rectangle kSetRestore{251, 402, 197, 27};
constexpr Rectangle SetDifficultyRow(int row) noexcept {
    return {503.0f, 202.0f + row * 36.0f, 400.0f, 32.0f};
}
inline EditorSetAction ClickedSetAction(Vector2 p) noexcept {
    if (CheckCollisionPointRec(p, kSetOpenFolder)) return EditorSetAction::OpenFolder;
    if (CheckCollisionPointRec(p, kSetBackground)) return EditorSetAction::AttachBackground;
    if (CheckCollisionPointRec(p, kSetRemoveBackground)) return EditorSetAction::RemoveBackground;
    if (CheckCollisionPointRec(p, kSetNew)) return EditorSetAction::NewDifficulty;
    if (CheckCollisionPointRec(p, kSetDuplicate)) return EditorSetAction::Duplicate;
    if (CheckCollisionPointRec(p, kSetImport)) return EditorSetAction::Import;
    if (CheckCollisionPointRec(p, kSetExport)) return EditorSetAction::Export;
    if (CheckCollisionPointRec(p, kSetDeleteDifficulty)) return EditorSetAction::DeleteDifficulty;
    if (CheckCollisionPointRec(p, kSetDeleteSong)) return EditorSetAction::DeleteSongSet;
    if (CheckCollisionPointRec(p, kSetRestore)) return EditorSetAction::Restore;
    return EditorSetAction::None;
}
constexpr Rectangle kFieldHeadingPrev{104, 235, 51, 47};
constexpr Rectangle kFieldHeading{162, 235, 234, 47};
constexpr Rectangle kFieldHeadingNext{403, 235, 51, 47};

// Absolute song milliseconds at the same fractional tick used by the route.
// The displayed value and copied clipboard value share this calculation.
constexpr Rectangle kRouteTimeValue{52, 440, 206, 23};
inline long long RouteTimeMs(const core::ChartEditor& draft, double activeTick) {
    const auto& loaded = draft.Data();
    const double tick = ViewBeat(draft, activeTick);
    const double seconds = loaded.offsetSeconds +
        tick * 60.0 / (loaded.route.bpm * loaded.route.ticksPerBeat);
    return static_cast<long long>(std::llround(seconds * 1000.0));
}

constexpr double kSpeeds[5] = {1.0, 0.75, 0.5, 0.25, 0.1};

// Conversion helpers for the fixed, song-wide timeline. This scale never
// depends on note zoom, BPM, snap subdivision or the active difficulty.
inline float SongOverviewX(double audioSeconds, double durationSeconds) noexcept {
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0)
        return kSongOverview.x;
    const double progress = std::clamp(audioSeconds / durationSeconds, 0.0, 1.0);
    return kSongOverview.x +
        static_cast<float>(progress * kSongOverview.width);
}
inline double SongOverviewSeconds(float x, double durationSeconds) noexcept {
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0)
        return 0.0;
    const double progress = std::clamp(
        static_cast<double>(x - kSongOverview.x) / kSongOverview.width,
        0.0, 1.0);
    return progress * durationSeconds;
}
inline int SongOverviewTick(const core::ChartEditor& draft,
                            double audioSeconds) noexcept {
    const auto& data = draft.Data();
    const auto& chart = data.route;
    const double ticks = (audioSeconds - data.offsetSeconds) *
        chart.bpm * chart.ticksPerBeat / 60.0;
    if (!std::isfinite(ticks)) return draft.SelectedBeat();
    const double bounded = std::clamp(ticks,
        static_cast<double>(chart.firstBeat),
        static_cast<double>(chart.lengthBeats - 1));
    return SnapTick(bounded, chart);
}

inline Rectangle RouteSpeedBounds(int index) {
    return {52.0f + index * 102.0f, 505.0f, 94.0f, 28.0f};
}

inline bool Hit(Rectangle bounds, Vector2 pos) { return CheckCollisionPointRec(pos, bounds); }

inline EditorTab ClickedTab(Vector2 mouse, EditorTab previous) {
    if (Hit(kTabSet, mouse)) return EditorTab::Set;
    if (Hit(kTabRoute, mouse)) return EditorTab::Route;
    if (Hit(kTabAudio, mouse)) return EditorTab::Audio;
    if (Hit(kTabField, mouse)) return EditorTab::Field;
    return previous;
}

 } // namespace locus::app::detail
