#pragma once

#include "app/game_settings.h"
#include "app/skin.h"
#include "core/chart_editor.h"
#include "raylib.h"

#include <limits>
#include <filesystem>
#include <vector>
#include <string>

namespace locus::app {

struct EditorControls {
    bool previousBeat = false, nextBeat = false;
    bool previousBar = false, nextBar = false; // Four beats = one 4/4 bar.
    // Held Ctrl+Left/Right or Ctrl+PgUp/PgDn for deterministic navigation.
    // UI input polls the key state; tests can supply elapsedSeconds directly.
    int heldBarDirection = 0; // -1 = left, +1 = right, 0 = released.
    float elapsedSeconds = 0.0f;
    bool movePrevious = false, moveNext = false;
    bool removeTurn = false;
    bool headingLeft = false, headingRight = false;
    bool mouseClick = false, mouseHeld = false, mouseReleased = false;
    bool shiftHeld = false;
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
    int speedIndex = -1; // 0..4: 100, 75, 50, 25, 10 percent.
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
    const core::LoadedChart* chart = nullptr; // Borrowed from SongLibrary for this frame.
    std::uintmax_t fileSize = 0;
    std::filesystem::file_time_type modified{};
};

struct EditorSetView {
    bool managed = false;
    bool newDifficultyDraft = false;
    bool hasBackground = false;
    std::filesystem::path backgroundPath;
    int previewStartMs = 0;
    std::string title, artist, audioName, id;
    std::vector<std::string> difficulties;
    std::vector<EditorComparisonOption> comparisonOptions;
    int current = -1, selected = -1, firstVisible = 0;
};
enum class EditorField { None, Title, Bpm, OffsetMs, GridSize };

struct EditorUiState {
    EditorField active = EditorField::None;
    EditorTab tab = EditorTab::Route;
    std::string buffer;
    bool replaceOnType = false;
    std::string feedback;
    bool feedbackError = false;
    int rangeFirst = -1, rangeLast = -1; // Inclusive selected beats.
    bool draggingTimeline = false;
    int dragAnchorBeat = 0;
    double dragViewBeat = 0.0;
    int heldBarDirection = 0;
    float heldBarSeconds = 0.0f;
    float barRepeatSeconds = 0.0f;
    core::ChartEditor::Pattern clipboard;
    bool HasRange() const noexcept {
        return rangeFirst >= 0 && rangeLast >= rangeFirst;
    }
    int setSelected = -1; // -1 follows the currently open difficulty.
    int setFirstVisible = 0;
    bool editingSetPreview = false;
    bool setPreviewReplaceOnType = false;
    std::string setPreviewBuffer;
    // Reference charts are identities, not song-library indices: rating
    // recalculation can reorder the library without changing the selection.
    std::vector<std::filesystem::path> comparisonFiles; // At most 3.
    bool comparisonPickerOpen = false;
    int comparisonPickerFirst = 0;
    int timelineZoomIndex = 3; // Default 36 px/beat; zoom changes view only.
    bool EditingText() const noexcept {
        return active != EditorField::None || editingSetPreview;
    }
};

struct EditorUiAction {
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
EditorControls PollEditorControls(const GameSettings& settings);
EditorUiAction ApplyEditorControls(core::ChartEditor& draft, EditorUiState& ui,
                                  const EditorControls& controls);
void RenderChartEditor(Font font, const core::ChartEditor& draft,
                       const EditorUiState& ui, const std::string& songName,
                       bool hasAudio, bool previewPlaying, bool previewPaused,
                       double previewBeat, double playbackSpeed,
                       const GameSettings& settings, const Skin& skin,
                       const EditorSetView& set = {},
                       const struct EditorComparisonFrame* comparisons = nullptr,
                       double previewSeconds = 0.0);

} // namespace locus::app
