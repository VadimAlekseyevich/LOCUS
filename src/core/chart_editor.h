#pragma once

#include "core/chart_json.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace locus::core {

// Editor values are owned in memory; platform adapters handle persistent files.
class ChartEditor {
public:
    // Pattern notes use beats relative to the beginning of a selected range.
    struct Pattern {
        std::vector<TurnEvent> notes;
        int span = 0;
    };
    ChartEditor();
    void NewDraft();
    void Load(const LoadedChart& source);
    bool SetSongDuration(double seconds);
    bool HasSongDuration() const noexcept { return songDurationSeconds_ > 0.0; }

    const LoadedChart& Data() const noexcept { return chart_; }
    int SelectedBeat() const noexcept { return selectedBeat_; }
    // Actual song timestamp for a selected beat and its signed offset.
    double AudioSecondsAtBeat(int beat) const noexcept;
    bool Dirty() const noexcept { return dirty_; }
    void MarkSaved() noexcept;
    bool SetAudioPath(const std::string& relativePath);

    bool SelectBeat(int beat) noexcept;
    bool StepSelection(int deltaBeats) noexcept; // Clamped; does not modify chart.
    bool ChangeBpm(int delta); // No arbitrary upper BPM cap.
    bool SetBpm(double bpm);
    bool ChangeOffsetMs(int deltaMs); // signed milliseconds
    bool SetOffsetMs(double milliseconds);
    // Grid width and height are authored gameplay geometry, not time signature.
    // Reject a resize that would silently move an authored starting cell.
    bool SetGridSize(int width, int height);
    bool SetStart(Cell cell);
    void RotateHeading(int quarterTurns);
    bool ChangeLength(int deltaBeats); // no deletion of events
    bool SetLength(int beats);
    bool SetTitle(const std::string& title);
    void PutTurn(Turn turn);
    bool RemoveTurn();
    bool MoveTurn(int beats); // preserves sorted, unique turn events
    const TurnEvent* SelectedTurn() const noexcept;

    Pattern CopyTurns(int firstBeat, int lastBeat) const;
    bool PasteTurns(int destinationBeat, const Pattern& pattern);
    bool ShiftTurns(int firstBeat, int lastBeat, int deltaBeats);
    bool InvertTurns(int firstBeat, int lastBeat);
    bool DeleteTurns(int firstBeat, int lastBeat);
    bool Undo();
    bool Redo();
    bool CanUndo() const noexcept { return !undo_.empty(); }
    bool CanRedo() const noexcept { return !redo_.empty(); }

private:
    struct Snapshot {
        LoadedChart chart;
        int selectedBeat = 0;
        std::uint64_t revision = 0;
    };
    void SaveUndo();
    void MarkDirty() noexcept;
    void ResetHistory() noexcept;
    bool ReflowSongRange() noexcept;
    static constexpr std::size_t kMaxHistory = 64;

    LoadedChart chart_;
    double songDurationSeconds_ = 0.0;
    int selectedBeat_ = 0;
    bool dirty_ = false;
    std::uint64_t revision_ = 0;
    std::uint64_t savedRevision_ = 0;
    std::uint64_t nextRevision_ = 0;
    std::deque<Snapshot> undo_;
    std::deque<Snapshot> redo_;
};

} // namespace locus::core
