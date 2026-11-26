#pragma once

#include "app/chart_editor_ui.h"
#include "core/chart_json.h"
#include "core/tempo_map.h"
#include "core/metronome.h"
#include "core/tap_tempo.h"

#include <string>
#include <future>

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
    void UpdateAudioAnalysis();
    void RecordTempoTap(double now);
    void ResetTempoTap() noexcept;
    std::vector<core::TempoMarker> TappedTempoMap() const;
    // An empty result is also the stale/invalid guard for UI commands.
    std::vector<core::TempoMarker> ProposedTempoMap(int index,bool replace) const;
    void ToggleMetronome() noexcept { ui_.metronomeEnabled=!ui_.metronomeEnabled; metronome_.Reset(); }
    core::MetronomeClick UpdateMetronome(double now) noexcept;
    void ResetMetronome() noexcept { metronome_.Reset(); }
    // Tempo-table commits invalidate the configured playback beat clock.
    // Stop rather than letting the old and new BPM rulers diverge mid-song.
    void StopPreviewForTempoChange();
    std::vector<core::TempoMarker> TempoMap() const {
        return core::EffectiveTempoMap(editor_.Data().route,
                                       editor_.Data().offsetSeconds);
    }

private:
    void StartPreview(int beat, double now);

    core::MetronomeCursor metronome_;
    core::TapTempo tapTempo_;
    std::uint64_t tapRevision_=0;
    std::future<std::shared_ptr<const core::EditorAudioAnalysis>> waveformJob_;
    std::uint64_t waveformRevision_=~std::uint64_t{0};
    std::uint64_t jobRevision_=0;
    std::shared_ptr<const core::EditorAudioAnalysis> analysis_;
    bool analysisFailed_=false;
    core::ChartEditor& editor_;
    EditorUiState& ui_;
    ChartPlayback& playback_;
    LocalMusic& music_;
    std::wstring& audioPath_;
    int& previewBeat_;
    std::string& status_;
};

} // namespace locus::app
