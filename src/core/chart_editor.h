#pragma once

#include "core/chart_json.h"

namespace locus::core {

// Editor values are owned in memory; platform adapters handle persistent files.
class ChartEditor {
public:
    ChartEditor();
    void NewDraft();
    void Load(const LoadedChart& source);

    const LoadedChart& Data() const noexcept { return chart_; }
    int SelectedBeat() const noexcept { return selectedBeat_; }
    bool Dirty() const noexcept { return dirty_; }
    void MarkSaved() noexcept { dirty_ = false; }
    bool SetAudioPath(const std::string& relativePath);

    bool SelectBeat(int beat) noexcept;
    bool ChangeBpm(int delta) noexcept; // integer BPM, 30..300
    bool SetBpm(double bpm) noexcept;
    bool ChangeOffsetMs(int deltaMs) noexcept; // signed milliseconds
    bool SetOffsetMs(double milliseconds) noexcept;
    bool SetStart(Cell cell) noexcept;
    void RotateHeading(int quarterTurns) noexcept;
    bool ChangeLength(int deltaBeats) noexcept; // no deletion of events
    bool SetLength(int beats) noexcept;
    bool SetTitle(const std::string& title);
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
