#pragma once

#include "app/chart_editor_ui.h"
#include "core/tempo_map.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace locus::app::detail {
constexpr float kVirtualWidth = 960.0f, kVirtualHeight = 540.0f;
constexpr float kGridCenterX = 448.0f, kGridCenterY = 282.0f;
constexpr float kGridMaxSpan = 240.0f;
// Zoomable, note-bearing ruler sits beneath the editor tabs.
// The separate full-song overview is always mapped to actual audio seconds.
constexpr float kTimelineX = 52.0f, kTimelineY = 117.0f;
constexpr float kTimelineWidth = 660.0f, kTimelineHeight = 47.0f;
constexpr Rectangle kEditorPreviewPlay{52, 485, 39, 40};
constexpr Rectangle kEditorPreviewRestart{96, 485, 39, 40};
constexpr Rectangle kSongOverview{145, 485, 570, 40};
constexpr Rectangle kEditorRateDropdown{725, 485, 93, 40};
constexpr Rectangle kEditorRatePopup{725, 341, 93, 140};
inline Rectangle EditorRateOption(int i) noexcept {
    return {729.0f, 345.0f + i * 27.0f, 85.0f, 25.0f};
}
// Fixed playhead at one third of the ruler; the fractional-beat ruler moves.
constexpr float kPlayheadX = kTimelineX + kTimelineWidth / 3.0f;
constexpr float kZoomPixelsPerBeat[7] = {12, 18, 26, 36, 52, 76, 104};
constexpr int kZoomMax = 6;
constexpr float kDragEdgeZone = 48.0f;
// Repeat a four-beat step quickly after a short delay; accelerate on hold.
constexpr float kBarRepeatDelay = 0.23f;
constexpr float kBarRepeatInterval = 0.075f;
constexpr float kBarRepeatFastInterval = 0.04f;
constexpr Rectangle kSnapLess{846, 118, 37, 22};
constexpr Rectangle kSnapMore{888, 118, 37, 22};
constexpr Rectangle kZoomOut{846, 143, 37, 22};
constexpr Rectangle kZoomIn{888, 143, 37, 22};
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

constexpr Rectangle kTabSet{433, 69, 127, 37};
constexpr Rectangle kTabRoute{567, 69, 145, 37};
constexpr Rectangle kTabAudio{719, 69, 145, 37};
constexpr Rectangle kEditorBack{52, 69, 99, 37};
constexpr Rectangle kSong{66, 412, 270, 35};
constexpr Rectangle kPlay = kEditorPreviewPlay;
constexpr Rectangle kStop = kEditorPreviewRestart;
constexpr Rectangle kTest{0, 0, 0, 0};
// Eight directions around DEL (center); positioned above the timeline.
constexpr Rectangle DirectionPadBounds(int row, int col) noexcept {
    return {748.0f + col * 63.0f, 181.0f + row * 60.0f, 58.0f, 55.0f};
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

// Vertical icon rail: clipboard/undo/redo remain keyboard-only.
constexpr Rectangle kMirrorHorizontal{52, 181, 42, 42};
constexpr Rectangle kMirrorVertical{52, 227, 42, 42};
constexpr Rectangle kRotateCounterclockwise{52, 273, 42, 42};
constexpr Rectangle kRotateClockwise{52, 319, 42, 42};
constexpr Rectangle kCompareToggle{52, 365, 42, 42};
constexpr Rectangle kRouteTest{52, 411, 42, 42};
constexpr Rectangle kComparePopup{105, 243, 252, 211};
constexpr Rectangle ComparePickerRow(int row) noexcept {
    return {115.0f, 266.0f + row * 29.0f, 232.0f, 25.0f};
}
constexpr Rectangle kComparePickerPrev{118, 420, 78, 21};
constexpr Rectangle kComparePickerNext{266, 420, 78, 21};

// Each preview remains square; shared comparison layouts never shift slots.
constexpr float kPreviewLeft = 106.0f;
constexpr float kPreviewRight = 733.0f;
constexpr float kPreviewTop = 179.0f;
constexpr float kPreviewMaxSide = 280.0f;
constexpr float kPreviewGap = 8.0f;
inline Rectangle ComparisonPreviewBounds(int count, int index) noexcept {
    const int visible = std::clamp(count, 1, 3);
    if (index < 0 || index >= visible) return {};
    const float width = kPreviewRight - kPreviewLeft;
    const float side = std::min(kPreviewMaxSide,
        (width - (visible - 1) * kPreviewGap) / visible);
    const float total = side * visible + (visible - 1) * kPreviewGap;
    return {kPreviewLeft + (width - total) * 0.5f +
                index * (side + kPreviewGap),
            kPreviewTop + (kPreviewMaxSide - side) * 0.5f, side, side};
}
constexpr Rectangle kEditorSpeedMinus{852, 408, 36, 28};
constexpr Rectangle kEditorSpeedPlus{894, 408, 36, 28};
constexpr Rectangle kEditorPathMinus{852, 444, 36, 28};
constexpr Rectangle kEditorPathPlus{894, 444, 36, 28};
inline int EditorSpeedControlDelta(Vector2 point) noexcept {
    return CheckCollisionPointRec(point, kEditorSpeedMinus) ? -1
         : CheckCollisionPointRec(point, kEditorSpeedPlus) ? 1 : 0;
}
inline int EditorPathControlDelta(Vector2 point) noexcept {
    return CheckCollisionPointRec(point, kEditorPathMinus) ? -1
         : CheckCollisionPointRec(point, kEditorPathPlus) ? 1 : 0;
}
constexpr Rectangle kStartHeadingPrev{817, 367, 34, 35};
constexpr Rectangle kStartHeadingValue{854, 367, 37, 35};
constexpr Rectangle kStartHeadingNext{896, 367, 34, 35};
constexpr Rectangle kAudioMetaPanel{52, 178, 302, 292};
constexpr Rectangle kAudioTempoPanel{367, 178, 566, 292};
constexpr Rectangle kTitle{66, 219, 270, 39};
constexpr Rectangle kAudioPreviewMs{66, 306, 270, 39};
constexpr Rectangle TempoOffsetCell(int row) noexcept {
    return {384.0f, 242.0f + row * 35.0f, 197.0f, 31.0f};
}
constexpr Rectangle TempoBpmCell(int row) noexcept {
    return {590.0f, 242.0f + row * 35.0f, 196.0f, 31.0f};
}
constexpr Rectangle TempoDeleteCell(int row) noexcept {
    return {797.0f, 242.0f + row * 35.0f, 122.0f, 31.0f};
}
constexpr Rectangle kTempoAdd{384, 427, 319, 30};
constexpr Rectangle kTempoPrev{773, 186, 68, 28};
constexpr Rectangle kTempoNext{851, 186, 68, 28};
constexpr Rectangle kTempoTable{380, 236, 540, 182};
// Song Set editor: metadata on the left, difficulties on the right.
// These rectangles are shared by drawing, hit-testing and input ownership.
constexpr Rectangle kSetTitleField{52, 194, 506, 35};
constexpr Rectangle kSetArtistField{52, 248, 246, 35};
constexpr Rectangle kSetMapperField{312, 248, 246, 35};
constexpr Rectangle kSetTagsField{52, 303, 506, 35};
constexpr Rectangle SetMetadataBounds(EditorSetField field) noexcept {
    switch (field) {
        case EditorSetField::Title: return kSetTitleField;
        case EditorSetField::Artist: return kSetArtistField;
        case EditorSetField::Mapper: return kSetMapperField;
        case EditorSetField::Tags: return kSetTagsField;
        case EditorSetField::None: return {0, 0, 0, 0};
    }
    return {0, 0, 0, 0};
}
inline EditorSetField ClickedSetMetadataField(Vector2 p) noexcept {
    for (EditorSetField field : {EditorSetField::Title,
                                 EditorSetField::Artist,
                                 EditorSetField::Mapper,
                                 EditorSetField::Tags})
        if (CheckCollisionPointRec(p, SetMetadataBounds(field)))
            return field;
    return EditorSetField::None;
}
constexpr Rectangle kSetBackground{52, 360, 246, 29};
constexpr Rectangle kSetRemoveBackground{312, 360, 246, 29};
constexpr Rectangle kSetImport{52, 397, 246, 29};
constexpr Rectangle kSetExport{312, 397, 246, 29};
constexpr Rectangle kSetOpenFolder{52, 434, 246, 29};
constexpr Rectangle kSetRestore{312, 434, 246, 29};
constexpr Rectangle kSetNew{570, 179, 170, 31};
constexpr Rectangle kSetDuplicate{747, 179, 183, 31};
constexpr Rectangle kSetDeleteDifficulty{570, 426, 170, 31};
constexpr Rectangle kSetDeleteSong{747, 426, 183, 31};
constexpr Rectangle SetDifficultyRow(int row) noexcept {
    return {570.0f, 221.0f + row * 39.0f, 360.0f, 36.0f};
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
constexpr Rectangle kFieldHeadingPrev = kStartHeadingPrev;
constexpr Rectangle kFieldHeading = kStartHeadingValue;
constexpr Rectangle kFieldHeadingNext = kStartHeadingNext;

// Absolute song milliseconds at the same fractional tick used by the route.
// The displayed value and copied clipboard value share this calculation.
constexpr Rectangle kRouteTimeValue{827, 485, 105, 40};
inline long long RouteTimeMs(const core::ChartEditor& draft, double activeTick) {
    const auto& loaded = draft.Data();
    const double tick = ViewBeat(draft, activeTick);
    const double seconds = core::TempoSecondsAtTick(
        loaded.route, loaded.offsetSeconds, tick);
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
    const double ticks = core::TempoBeatAtSeconds(
        chart, data.offsetSeconds, audioSeconds) * chart.ticksPerBeat;
    if (!std::isfinite(ticks)) return draft.SelectedBeat();
    const double bounded = std::clamp(ticks,
        static_cast<double>(chart.firstBeat),
        static_cast<double>(chart.lengthBeats - 1));
    return SnapTick(bounded, chart);
}

inline Rectangle RouteSpeedBounds(int index) {
    return EditorRateOption(index);
}

inline bool Hit(Rectangle bounds, Vector2 pos) { return CheckCollisionPointRec(pos, bounds); }

inline EditorTab ClickedTab(Vector2 mouse, EditorTab previous) {
    if (Hit(kTabSet, mouse)) return EditorTab::Set;
    if (Hit(kTabRoute, mouse)) return EditorTab::Route;
    if (Hit(kTabAudio, mouse)) return EditorTab::Audio;
    return previous;
}

 } // namespace locus::app::detail
