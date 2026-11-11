#pragma once

#include "app/chart_editor_ui.h"
#include "core/chart_json.h"

#include <string>

namespace locus::core { class TurnJudge; }
namespace locus::app {
class ChartPlayback;
class LocalMusic;
class GameSession;

// Validation is independent of raylib's audio device. A failed plan never
// modifies the output or the active chart; gameplay commits only after it
// knows audio is attached and the selected position is within its duration.
struct EditorTestPlan {
    core::LoadedChart chart;
    double audioSeconds = 0.0;
    double exactBeat = 0.0;
    int firstScoredBeat = 0;
};

bool PrepareEditorTest(const core::LoadedChart& source, double audioSeconds,
                       double durationSeconds, EditorTestPlan& plan,
                       std::string& error);

struct EditorTestTarget {
    core::Chart& activeChart; // Identity must remain stable for judge/session.
    std::string& title;
    core::TurnJudge& judge;
    GameSession& gameSession;
    bool& customChart;
};

// Coordinates ROUTE editor input and its independent audio preview, without
// owning the editor document, the music decoder or the GUI screen.
// The caller retains control of native file pickers and navigation.
class EditorSession {
public:
    EditorSession(core::ChartEditor& editor, EditorUiState& ui,
                  ChartPlayback& playback, LocalMusic& music,
                  std::wstring& editorAudioPath, int& previewStartBeat,
                  std::string& status) noexcept
        : editor_(editor), ui_(ui), playback_(playback), music_(music),
          audioPath_(editorAudioPath), previewBeat_(previewStartBeat),
          status_(status) {}

    EditorUiAction ApplyControls(const EditorControls& input, double now);
    bool BeginTest(EditorTestTarget target, double now, std::string& error);
    void UpdatePreview(double now);
    // Tempo-table commits invalidate the configured playback beat clock.
    // Stop rather than letting the old and new BPM rulers diverge mid-song.
    void StopPreviewForTempoChange();

private:
    void StartPreview(int beat, double now);

    core::ChartEditor& editor_;
    EditorUiState& ui_;
    ChartPlayback& playback_;
    LocalMusic& music_;
    std::wstring& audioPath_;
    int& previewBeat_;
    std::string& status_;
};

} // namespace locus::app
