#pragma once

#include "core/chart_json.h"

namespace locus::core {

// In-memory editor draft. M-03 deliberately does not save files; M-04 will
// connect this same value object to save/reopen and game test-play.
class ChartEditor {
public:
    ChartEditor();
    void NewDraft();
    void Load(const LoadedChart& source);

    const LoadedChart& Data() const noexcept { return chart_; }
    int SelectedBeat() const noexcept { return selectedBeat_; }
    bool Dirty() const noexcept { return dirty_; }

    bool SelectBeat(int beat) noexcept;
    bool ChangeBpm(int delta) noexcept; // integer BPM, 30..300
    bool ChangeOffsetMs(int deltaMs) noexcept; // signed milliseconds
    bool SetStart(Cell cell) noexcept;
    void RotateHeading(int quarterTurns) noexcept;
    bool ChangeLength(int deltaBeats) noexcept; // no deletion of events
    void PutTurn(Turn turn);
    bool RemoveTurn() noexcept;
    bool MoveTurn(int beats) noexcept; // preserves sorted, unique turn events
    const TurnEvent* SelectedTurn() const noexcept;

private:
    void MarkDirty() noexcept { dirty_ = true; }

    LoadedChart chart_;
    int selectedBeat_ = 0;
    bool dirty_ = false;
};

} // namespace locus::core
