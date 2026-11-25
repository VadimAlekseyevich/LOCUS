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
constexpr float kTimelineX = 52.0f, kTimelineY = 17.0f;
constexpr float kTimelineWidth = 660.0f, kTimelineHeight = 47.0f;
inline Rectangle TimelineBounds(const EditorUiState& ui) noexcept {
    return HasLayoutArea(ui.layout.timeline) ? ui.layout.timeline :
        Rectangle{kTimelineX, kTimelineY, kTimelineWidth, kTimelineHeight};
}
inline float PlayheadX(const EditorUiState& ui) noexcept {
    const auto bounds = TimelineBounds(ui);
    return bounds.x + bounds.width / 3.0f;
}
constexpr Rectangle kEditorPreviewPlay{52, 485, 39, 40};
constexpr Rectangle kEditorPreviewRestart{96, 485, 39, 40};
constexpr Rectangle kSongOverview{145, 485, 570, 40};
inline Rectangle OverviewBounds(const EditorUiState& ui) noexcept {
    return HasLayoutArea(ui.layout.overview) ? ui.layout.overview : kSongOverview;
}
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
constexpr Rectangle kSnapLess{846, 18, 37, 22};
constexpr Rectangle kSnapMore{888, 18, 37, 22};
constexpr Rectangle kZoomOut{846, 43, 37, 22};
constexpr Rectangle kZoomIn{888, 43, 37, 22};
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

// Upper ruler: fixed SONG-TIME scale. A BPM change changes how tightly
// musical grid lines are packed, never the rate at which the ruler scrolls.
// Zoom retains its original meaning at the first BPM.
inline double TimelinePixelsPerSongSecond(
    const EditorUiState& ui, const core::Chart& chart) noexcept {
    return PixelsPerBeat(ui) * chart.bpm / 60.0;
}
inline double TimelineAnchorSeconds(
    const core::ChartEditor& draft, double anchorTick) noexcept {
    const auto& loaded = draft.Data();
    return core::TempoSecondsAtTick(
        loaded.route, loaded.offsetSeconds, anchorTick);
}
inline float TimelineXAtSeconds(
    const core::ChartEditor& draft, const EditorUiState& ui,
    double anchorTick, double songSeconds) noexcept {
    const double px = TimelinePixelsPerSongSecond(ui, draft.Data().route);
    return PlayheadX(ui) + static_cast<float>(
        (songSeconds - TimelineAnchorSeconds(draft, anchorTick)) * px);
}
inline double TimelineSecondsAtX(
    const core::ChartEditor& draft, const EditorUiState& ui,
    double anchorTick, float x) noexcept {
    const double px = TimelinePixelsPerSongSecond(ui, draft.Data().route);
    return TimelineAnchorSeconds(draft, anchorTick) +
        (static_cast<double>(x) - PlayheadX(ui)) / std::max(px, 1e-6);
}
inline int TimelineTickAtX(
    const core::ChartEditor& draft, const EditorUiState& ui,
    double anchorTick, float x) {
    const auto& loaded = draft.Data();
    return core::TempoSnapTickAtSeconds(
        loaded.route, loaded.offsetSeconds,
        TimelineSecondsAtX(draft, ui, anchorTick, x));
}

// Hit-test the same authoritative millisecond coordinates DrawTimeline uses.
// A note may lie BETWEEN snap marks; never map a clicked note through a
// snapped tick first. Binary search avoids scanning offscreen chart turns.
inline std::optional<std::int64_t> TimelineNoteTimeAtX(
    const core::ChartEditor& draft, const EditorUiState& ui,
    double anchorTick, float x, float radiusPixels = 7.0f) {
    const auto bounds = TimelineBounds(ui);
    if (x < bounds.x + 8.0f ||
        x > bounds.x + bounds.width - 8.0f) return std::nullopt;
    const auto& loaded = draft.Data();
    const auto& route = loaded.route;
    const auto& turns = route.turns;
    if (turns.empty()) return std::nullopt;
    const double candidateMs = TimelineSecondsAtX(
        draft, ui, anchorTick, x) * 1000.0;
    const auto after = std::lower_bound(
        turns.begin(), turns.end(), candidateMs,
        [&](const core::TurnEvent& turn, double timeMs) {
            return core::EventTimeMs(route, turn, loaded.offsetSeconds) < timeMs;
        });
    float distance = radiusPixels;
    std::optional<std::int64_t> found;
    const auto consider = [&](const core::TurnEvent& event) {
        const std::int64_t timeMs = core::EventTimeMs(
            route, event, loaded.offsetSeconds);
        const float at = TimelineXAtSeconds(
            draft, ui, anchorTick, timeMs / 1000.0);
        const float delta = std::abs(at - x);
        if (delta <= distance) {
            distance = delta;
            found = timeMs;
        }
    };
    if (after != turns.end()) consider(*after);
    if (after != turns.begin()) consider(*(after - 1));
    return found;
}

constexpr Rectangle kTabSet{433, 69, 127, 37};
constexpr Rectangle kTabRoute{567, 69, 145, 37};
constexpr Rectangle kTabAudio{719, 69, 145, 37};
constexpr Rectangle kEditorBack{52, 69, 99, 37};
constexpr Rectangle kSong{361, 151, 268, 30};
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

// Fixed legacy bounds are used only by the old non-native renderer and
// deterministic headless input fixtures. The native path supplies a slot.
constexpr float kPreviewLeft = 106.0f;
constexpr float kPreviewRight = 733.0f;
constexpr float kPreviewTop = kTimelineY + kTimelineHeight + 15.0f;
constexpr float kPreviewGap = 8.0f;
constexpr Rectangle kLegacyPreviewArea{
    kPreviewLeft, kPreviewTop, kPreviewRight - kPreviewLeft,
    kSongOverview.y - kPreviewTop - 26.0f};
inline Rectangle PreviewBounds(const EditorUiState& ui) noexcept {
    return HasLayoutArea(ui.layout.preview) ? ui.layout.preview : kLegacyPreviewArea;
}
inline Rectangle ComparisonPreviewBounds(const EditorUiState& ui, int count, int index) noexcept {
    return SquarePreviewPane(PreviewBounds(ui), count, index, kPreviewGap);
}
inline Rectangle ComparisonPreviewBounds(int count, int index) noexcept {
    return SquarePreviewPane(kLegacyPreviewArea, count, index, kPreviewGap);
}
inline Rectangle CompareToggleBounds(const EditorUiState& ui) noexcept {
    return HasLayoutArea(ui.layout.compareToggle) ? ui.layout.compareToggle : kCompareToggle;
}
inline Rectangle ComparePopupBounds(const EditorUiState& ui) noexcept {
    if (!HasLayoutArea(ui.layout.compareToggle)) return kComparePopup;
    const auto button = CompareToggleBounds(ui);
    const auto area = PreviewBounds(ui);
    return {button.x + button.width + 10.0f,
            std::clamp(button.y, area.y, std::max(area.y, area.y + area.height-kComparePopup.height)),
            kComparePopup.width, kComparePopup.height};
}
inline Rectangle ComparePickerRow(const EditorUiState& ui, int row) noexcept {
    const auto popup = ComparePopupBounds(ui);
    return {popup.x+10.0f,popup.y+23.0f+row*29.0f,popup.width-20.0f,25.0f};
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
constexpr Rectangle kSetupMetadataPanel{43,117,300,353};
constexpr Rectangle kAudioTempoPanel{350,117,290,353};
constexpr Rectangle kSetupDifficultyPanel{647,117,290,353};
constexpr Rectangle kAudioMetaPanel = kAudioTempoPanel;
// Retired standalone fields are not interactive; old enum cases are inert.
constexpr Rectangle kBpm{-1, -1, 0, 0};
constexpr Rectangle kOffset{-1, -1, 0, 0};
constexpr Rectangle kTitle{658, 155, 268, 30};
constexpr Rectangle kAudioPreviewMs{361, 209, 268, 30};
constexpr Rectangle kSetupEnter{658, 192, 268, 31};
constexpr Rectangle TempoOffsetCell(int row) noexcept {
    return {361.0f, 281.0f + row * 28.0f, 105.0f, 25.0f};
}
constexpr Rectangle TempoBpmCell(int row) noexcept {
    return {471.0f, 281.0f + row * 28.0f, 88.0f, 25.0f};
}
constexpr Rectangle TempoDeleteCell(int row) noexcept {
    return {564.0f, 281.0f + row * 28.0f, 65.0f, 25.0f};
}
constexpr Rectangle kTempoAdd{361, 431, 268, 30};
constexpr Rectangle kTempoTable{361, 281, 268, 140};
// Song Set editor: metadata on the left, difficulties on the right.
// These rectangles are shared by drawing, hit-testing and input ownership.
constexpr Rectangle kSetTitleField{54, 155, 278, 30};
constexpr Rectangle kSetArtistField{54, 200, 278, 30};
constexpr Rectangle kSetMapperField{54, 245, 278, 30};
constexpr Rectangle kSetTagsField{54, 290, 278, 30};
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
constexpr Rectangle kSetBackground{54, 333, 136, 30};
constexpr Rectangle kSetRemoveBackground{196, 333, 136, 30};
constexpr Rectangle kSetImport{54, 371, 136, 30};
constexpr Rectangle kSetExport{196, 371, 136, 30};
constexpr Rectangle kSetOpenFolder{54, 409, 136, 30};
constexpr Rectangle kSetRestore{196, 409, 136, 30};
constexpr Rectangle kSetNew{658, 229, 130, 28};
constexpr Rectangle kSetDuplicate{796, 229, 130, 28};
constexpr Rectangle kSetDeleteDifficulty{658, 431, 130, 30};
constexpr Rectangle kSetDeleteSong{796, 431, 130, 30};
constexpr Rectangle SetDifficultyRow(int row) noexcept {
    return {658.0f, 264.0f + row * 32.0f, 268.0f, 29.0f};
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
inline float SongOverviewX(double audioSeconds, double durationSeconds,
                           Rectangle bounds = kSongOverview) noexcept {
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0)
        return bounds.x;
    const double progress = std::clamp(audioSeconds / durationSeconds, 0.0, 1.0);
    return bounds.x + static_cast<float>(progress * bounds.width);
}
inline double SongOverviewSeconds(float x, double durationSeconds,
                                  Rectangle bounds = kSongOverview) noexcept {
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0)
        return 0.0;
    const double progress = std::clamp(
        static_cast<double>(x - bounds.x) / std::max(1.0f, bounds.width),
        0.0, 1.0);
    return progress * durationSeconds;
}
inline int SongOverviewTick(const core::ChartEditor& draft,
                            double audioSeconds) noexcept {
    const auto& data = draft.Data();
    return core::TempoSnapTickAtSeconds(
        data.route, data.offsetSeconds, audioSeconds);
}

inline Rectangle RouteSpeedBounds(int index) {
    return EditorRateOption(index);
}

inline bool Hit(Rectangle bounds, Vector2 pos) { return CheckCollisionPointRec(pos, bounds); }

// Navigation is an explicit workspace transition, not a tab hitbox.
inline EditorTab ClickedTab(Vector2, EditorTab previous) { return previous; }

 } // namespace locus::app::detail
