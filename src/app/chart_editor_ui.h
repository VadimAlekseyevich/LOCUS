#pragma once

#include "app/game_settings.h"
#include "app/editor_layout_snapshot.h"
#include "app/skin.h"
#include "core/chart_editor.h"
#include "raylib.h"

#include <limits>
#include <memory>
#include <cstdint>
#include <functional>
#include <optional>
#include <filesystem>
#include <vector>
#include <string>

namespace locus::core { class AudioWaveform; }

namespace locus::app {

enum class EditorSetAction;
enum class EditorSetField;

enum class ComparePickerAction { None, Toggle, Dismiss, Select };

struct EditorControls {
    bool previousBeat = false, nextBeat = false;
    bool previousBar = false, nextBar = false; // Four beats = one 4/4 bar.
    // Held Ctrl+Left/Right or Ctrl+PgUp/PgDn for deterministic navigation.
    // UI input polls the key state; tests can supply elapsedSeconds directly.
    int heldBarDirection = 0; // -1 = left, +1 = right, 0 = released.
    float elapsedSeconds = 0.0f;
    bool movePrevious = false, moveNext = false;
    bool removeTurn = false;
    bool mouseClick = false, mouseHeld = false, mouseReleased = false;
    bool shiftHeld = false;
    bool ctrlHeld = false; // Ctrl+click the note ruler to stage exact milliseconds.
    Vector2 mouse{};
    float mouseWheel = 0.0f;
    // NaN means use the selected beat. -1 is a legitimate pre-offset beat.
    double timelineViewBeat = std::numeric_limits<double>::quiet_NaN();
    bool zoomIn = false, zoomOut = false;
    bool snapMore = false, snapLess = false; // Placement grid, NOT BPM or zoom.
    bool copyRange = false, cutRange = false, pasteRange = false;
    bool mirrorHorizontal = false, mirrorVertical = false;
    bool rotateClockwise = false, rotateCounterclockwise = false;
    bool undo = false, redo = false;
    bool shiftRangePrevious = false, shiftRangeNext = false;

    bool enter = false, escape = false, backspace = false, tab = false;
    bool previewToggle = false, previewRestart = false;
    bool metronomeToggle = false;
    bool copySongTime = false;
    // Semantic editor actions emitted by RmlUi; never synthesize a click at
    // legacy pixel coordinates to reach the chart editing model.
    std::optional<core::Heading> placeHeading;
    int rotateStartHeading = 0;
    int previewSpeedDelta = 0;
    int previewLengthDelta = 0;
    ComparePickerAction compareAction = ComparePickerAction::None;
    std::filesystem::path compareFile; // Stable file identity, never row index.
    // Typed Set editor operations. No synthetic raylib click coordinates.
    std::optional<EditorSetAction> setRequest;
    int selectedDifficulty = -1;
    std::string selectedDifficultyId;
    bool selectDraft = false;
    bool enterChart = false;
    bool attachAudio = false;
    std::optional<std::string> previewMsValue;
    int tempoRow = -1;
    bool tempoBpm = false, tempoAdd = false, tempoRemove = false;
    std::optional<std::string> tempoValue;
    std::optional<std::string> difficultyTitle;
    std::optional<EditorSetField> metadataField;
    std::string metadataValue;
    std::string metadataSetId;
    // True when the native RmlUi editor shell owns tabs and transport.
    // Legacy raylib hitboxes are forbidden in that mode, but hotkeys and
    // note/tempo editing retain their original semantics.
    bool nativeChrome = false;
    int speedIndex = -1; // Legacy speed key bindings.
    int speedPercent = 0; // RmlUi numeric transport rate, 1..100; 0=no change.
    std::string typedUtf8;
};

enum class EditorTab { Route, Audio, Field, Set };
enum class EditorSetAction {
    None, Open, NewDifficulty, Duplicate, Import, Export,
    DeleteDifficulty, DeleteSongSet, Restore,
    AttachBackground, RemoveBackground, SetPreviewExact, OpenFolder
};
struct EditorComparisonOption {
    std::filesystem::path file;
    std::string title;
    bool available = false;
    std::uintmax_t fileSize = 0;
    std::filesystem::file_time_type modified{};
    std::string gameplayRevision;
    std::uint64_t contentFingerprint = 0;
};

struct EditorSetView {
    bool managed = false;
    bool newDifficultyDraft = false;
    bool hasAudio = false;
    bool hasBackground = false;
    std::filesystem::path backgroundPath;
    int previewStartMs = 0;
    // Shared Song Set tempo marker, not per-difficulty note metadata.
    double bpm = 120.0;
    int offsetMs = 0;
    std::vector<core::TempoMarker> tempoMap; // Shared by all difficulties.
    std::string title, artist, mapper, audioName, id;
    std::vector<std::string> tags; // Set-wide metadata.
    std::vector<std::string> difficulties;
    std::vector<std::string> difficultyIds;
    std::vector<EditorComparisonOption> comparisonOptions;
    // Materialize only chosen references. The view never exposes note
    // storage or retains pointers into a catalog snapshot.
    std::function<bool(const std::filesystem::path&,core::LoadedChart&,
                       std::string&)> loadComparison;
    int current = -1, selected = -1, firstVisible = 0;
    // A published catalog refresh permits a retry of failed lazy loads
    // even if the difficulty's content fingerprint is unchanged.
    std::uint64_t catalogRevision = 0;
};
enum class EditorField { None, Title, Bpm, OffsetMs };
enum class EditorSetField { None, Title, Artist, Mapper, Tags };

struct EditorUiState {
    std::shared_ptr<const core::AudioWaveform> waveform;
    bool metronomeEnabled=false;
    EditorLayoutSnapshot layout; // presentation only; refreshed from RmlUi
    EditorField active = EditorField::None;
    EditorTab tab = EditorTab::Route;
    std::string buffer;
    bool replaceOnType = false;
    EditorSetField editingSetField = EditorSetField::None;
    std::string setFieldBuffer;
    bool setFieldReplaceOnType = false;
    std::string editingSetId;
    std::string feedback;
    bool feedbackError = false;
    int rangeFirst = -1, rangeLast = -1; // Inclusive selected beats.
    bool draggingTimeline = false;
    bool draggingSongOverview = false; // Seek only; never selects/edits notes.
    // A Ctrl+click only picks an exact song time; direction pad commits it.
    std::optional<std::int64_t> pendingExactPlacementTimeMs;
    int dragAnchorBeat = 0;
    double dragViewBeat = 0.0;
    int heldBarDirection = 0;
    float heldBarSeconds = 0.0f;
    float barRepeatSeconds = 0.0f;
    core::ChartEditor::Pattern clipboard;
    bool HasRange() const noexcept {
        return rangeFirst >= 0 && rangeLast >= rangeFirst;
    }
    int setSelected = -1; // fallback selection for older callers
    std::string setSelectedId; // explicit selection, independent of open document
    bool setupDraftSelected = false;
    int setFirstVisible = 0;
    int tempoEditingRow = -1;
    bool tempoEditingBpm = false;
    bool tempoReplaceOnType = false;
    std::string tempoBuffer;
    int tempoFirstVisible = 0;
    bool editingSetPreview = false;
    bool setPreviewReplaceOnType = false;
    std::string setPreviewBuffer;
    // Reference charts are identities, not song-library indices: rating
    // recalculation can reorder the library without changing the selection.
    std::vector<std::filesystem::path> comparisonFiles; // At most 2; visual order excluding the active chart.
    int comparisonFocusedSlot = 0; // Stable physical pane index of focused chart, [0..comparisonFiles.size()].
    bool comparisonPickerOpen = false;
    int comparisonPickerFirst = 0;
    bool rateDropdownOpen = false; // Preview transport rate, all editor tabs.
    int timelineZoomIndex = 3; // Default 36 px/beat; zoom changes view only.
    // Exact song-time anchor set by cross-difficulty focus. Each chart's
    // authoring cursor is projected to its own tick grid, but an idle
    // preview/ruler must stay at the SAME millisecond, not jump to a
    // rounded tick. Cleared on explicit navigation.
    double focusSongSeconds = std::numeric_limits<double>::quiet_NaN();
    bool EditingText() const noexcept {
        return active != EditorField::None || editingSetPreview ||
               tempoEditingRow >= 0 ||
               editingSetField != EditorSetField::None;
    }
};

enum class EditorHistoryOperation { None, Undo, Redo };

struct EditorUiAction {
    EditorHistoryOperation historyOperation = EditorHistoryOperation::None;
    bool chooseAudio = false;
    bool previewToggle = false;
    bool previewRestart = false;
    bool testPlay = false;
    bool changedChart = false;
    // The pure editor returns clipboard text; the outer screen adapter
    // decides when to publish it to the native OS clipboard.
    std::string clipboardWrite;
    int speedIndex = -1;
};

Vector2 EditorVirtualMouse(Vector2 physicalMouse, int screenWidth, int screenHeight);
EditorControls PollEditorControls(const GameSettings& settings,
                                  bool consumeText = true);
EditorUiAction ApplyEditorControls(core::ChartEditor& draft, EditorUiState& ui,
                                  const EditorControls& controls);
void RenderChartEditor(Font font, const core::ChartEditor& draft,
                       const EditorUiState& ui, const std::string& songName,
                       bool hasAudio, bool previewPlaying, bool previewPaused,
                       double previewBeat, double playbackSpeed,
                       const GameSettings& settings, const Skin& skin,
                       const EditorSetView& set = {},
                       const struct EditorComparisonFrame* comparisons = nullptr,
                       double previewSeconds = 0.0,
                       bool nativeShell = false,
                       struct PathRibbonWorkspace* ribbon = nullptr);

} // namespace locus::app
