#include "core/chart_editor.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace locus::core {

ChartEditor::ChartEditor() {
    NewDraft();
}

void ChartEditor::NewDraft() {
    chart_ = LoadedChart{};
    chart_.title = "Untitled Chart";
    chart_.audioPath = "song.mp3";
    chart_.route = Chart{};
    chart_.route.bpm = 120.0;
    chart_.route.lengthBeats = 32;
    chart_.authoredLengthBeats.reset();
    songDurationSeconds_ = 0.0;
    chart_.route.start = Cell{1, 1};
    chart_.route.initialHeading = Heading::Right;
    chart_.route.turns.clear();
    chart_.offsetSeconds = 0.0;
    selectedBeat_ = 0;
    ResetHistory();
}

void ChartEditor::Load(const LoadedChart& source) {
    chart_ = source;
    songDurationSeconds_ = 0.0;
    selectedBeat_ = std::clamp(0, chart_.route.firstBeat,
                              chart_.route.lengthBeats - 1);
    ResetHistory();
}

bool ChartEditor::SetSongDuration(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0) return false;
    const double old = songDurationSeconds_;
    songDurationSeconds_ = seconds;
    if (!ReflowSongRange()) {
        songDurationSeconds_ = old;
        return false;
    }
    return true;
}

bool ChartEditor::ReflowSongRange() noexcept {
    if (songDurationSeconds_ <= 0.0) return true; // Temporary unbound draft.
    int first = 0, end = 0;
    std::string error;
    if (!ComputeSongBeatRange(chart_.route.bpm, chart_.offsetSeconds,
                              songDurationSeconds_, first, end, error)) return false;
    if (!TurnRangeFits(first, end)) return false;
    chart_.route.firstBeat = first;
    chart_.route.lengthBeats = end;
    chart_.authoredLengthBeats.reset();
    selectedBeat_ = std::clamp(selectedBeat_, first, end - 1);
    return true;
}

bool ChartEditor::TurnRangeFits(int firstBeat, int endBeat) const noexcept {
    // A BPM/offset/audio change may not silently strand authored turns
    // outside the song's editable timeline.
    for (const auto& event : chart_.route.turns)
        if (event.beat < firstBeat || event.beat >= endBeat) return false;
    return true;
}

void ChartEditor::ResetHistory() noexcept {
    undo_.clear();
    redo_.clear();
    revision_ = savedRevision_ = nextRevision_ = 0;
    dirty_ = false;
}

void ChartEditor::MarkSaved() noexcept {
    savedRevision_ = revision_;
    dirty_ = false;
}

void ChartEditor::MarkDirty() noexcept {
    revision_ = ++nextRevision_;
    dirty_ = revision_ != savedRevision_;
}

void ChartEditor::SaveUndo() {
    if (undo_.size() == kMaxHistory) undo_.pop_front();
    undo_.push_back({chart_, selectedBeat_, revision_});
    redo_.clear();
}

bool ChartEditor::Undo() {
    if (undo_.empty()) return false;
    if (redo_.size() == kMaxHistory) redo_.pop_front();
    redo_.push_back({chart_, selectedBeat_, revision_});
    Snapshot previous = std::move(undo_.back());
    undo_.pop_back();
    chart_ = std::move(previous.chart);
    selectedBeat_ = previous.selectedBeat;
    revision_ = previous.revision;
    dirty_ = revision_ != savedRevision_;
    return true;
}

bool ChartEditor::Redo() {
    if (redo_.empty()) return false;
    if (undo_.size() == kMaxHistory) undo_.pop_front();
    undo_.push_back({chart_, selectedBeat_, revision_});
    Snapshot next = std::move(redo_.back());
    redo_.pop_back();
    chart_ = std::move(next.chart);
    selectedBeat_ = next.selectedBeat;
    revision_ = next.revision;
    dirty_ = revision_ != savedRevision_;
    return true;
}

double ChartEditor::AudioSecondsAtBeat(int beat) const noexcept {
    return chart_.offsetSeconds +
           static_cast<double>(beat) * 60.0 / chart_.route.bpm;
}

bool ChartEditor::SetAudioPath(const std::string& relativePath) {
    if (!ValidChartAudioPath(relativePath)) return false;
    if (chart_.audioPath == relativePath) return true;
    SaveUndo();
    chart_.audioPath = relativePath;
    MarkDirty();
    return true;
}

bool ChartEditor::SelectBeat(int beat) noexcept {
    if (beat < chart_.route.firstBeat ||
        beat >= chart_.route.lengthBeats || selectedBeat_ == beat) return false;
    selectedBeat_ = beat;
    return true;
}

bool ChartEditor::StepSelection(int deltaBeats) noexcept {
    // Avoid out-of-range selections at either edge of a short timeline.
    const long long next = std::clamp(
        static_cast<long long>(selectedBeat_) + deltaBeats,
        static_cast<long long>(chart_.route.firstBeat),
        static_cast<long long>(chart_.route.lengthBeats - 1));
    return SelectBeat(static_cast<int>(next));
}

bool ChartEditor::SetBpm(double bpm) {
    // Chart JSON already accepts every finite positive BPM. The editor must
    // not reject legitimate fast charts (e.g. 400 BPM) at an arbitrary 300.
    if (!std::isfinite(bpm) || bpm <= 0.0) return false;
    if (bpm != chart_.route.bpm) {
        int first = chart_.route.firstBeat, end = chart_.route.lengthBeats;
        std::string error;
        if (HasSongDuration() &&
            !ComputeSongBeatRange(bpm, chart_.offsetSeconds,
                                  songDurationSeconds_, first, end, error)) return false;
        if (!TurnRangeFits(first, end)) return false;
        SaveUndo();
        chart_.route.bpm = bpm;
        chart_.route.firstBeat = first;
        chart_.route.lengthBeats = end;
        selectedBeat_ = std::clamp(selectedBeat_, first, end - 1);
        MarkDirty();
    }
    return true;
}

bool ChartEditor::SetOffsetMs(double milliseconds) {
    if (!std::isfinite(milliseconds) || std::abs(milliseconds) > 3600000.0)
        return false;
    if (milliseconds != chart_.offsetSeconds * 1000.0) {
        int first = chart_.route.firstBeat, end = chart_.route.lengthBeats;
        std::string error;
        if (HasSongDuration() &&
            !ComputeSongBeatRange(chart_.route.bpm, milliseconds / 1000.0,
                                  songDurationSeconds_, first, end, error)) return false;
        if (!TurnRangeFits(first, end)) return false;
        SaveUndo();
        chart_.offsetSeconds = milliseconds / 1000.0;
        chart_.route.firstBeat = first;
        chart_.route.lengthBeats = end;
        selectedBeat_ = std::clamp(selectedBeat_, first, end - 1);
        MarkDirty();
    }
    return true;
}

bool ChartEditor::SetTitle(const std::string& title) {
    if (title.empty() || title.size() > 100 ||
        title.find_first_not_of(" \t\n\r") == std::string::npos) return false;
    if (title != chart_.title) {
        SaveUndo();
        chart_.title = title;
        MarkDirty();
    }
    return true;
}

bool ChartEditor::ChangeBpm(int delta) {
    const double target = chart_.route.bpm + delta;
    return target != chart_.route.bpm && SetBpm(target);
}

bool ChartEditor::ChangeOffsetMs(int deltaMs) {
    constexpr long long kLimitMs = 3600000; // one hour; no overflow while editing
    const double before = chart_.offsetSeconds * 1000.0;
    if (!std::isfinite(before) || std::abs(before) > static_cast<double>(kLimitMs))
        return false;
    const long long current = std::llround(before);
    const long long target = current + static_cast<long long>(deltaMs);
    if (target < -kLimitMs || target > kLimitMs || target == current) return false;
    return SetOffsetMs(static_cast<double>(target));
}

bool ChartEditor::SetGridSize(int width, int height) {
    if (width < kMinGridSize || width > kMaxGridSize ||
        height < kMinGridSize || height > kMaxGridSize ||
        chart_.route.start.x >= width || chart_.route.start.y >= height) return false;
    if (width != chart_.route.gridWidth || height != chart_.route.gridHeight) {
        SaveUndo();
        chart_.route.gridWidth = width;
        chart_.route.gridHeight = height;
        MarkDirty();
    }
    return true;
}

bool ChartEditor::SetStart(Cell cell) {
    if (cell.x < 0 || cell.x >= chart_.route.gridWidth ||
        cell.y < 0 || cell.y >= chart_.route.gridHeight ||
        (cell.x == chart_.route.start.x && cell.y == chart_.route.start.y)) return false;
    SaveUndo();
    chart_.route.start = cell;
    MarkDirty();
    return true;
}

void ChartEditor::RotateHeading(int quarterTurns) {
    const int delta = (quarterTurns % 4 + 4) % 4;
    if (delta == 0) return;
    SaveUndo();
    chart_.route.initialHeading = static_cast<Heading>(
        (static_cast<int>(chart_.route.initialHeading) + delta) % 4);
    MarkDirty();
}

const TurnEvent* ChartEditor::SelectedTurn() const noexcept {
    const auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    return (found != events.end() && found->beat == selectedBeat_) ? &*found : nullptr;
}

void ChartEditor::PutTurn(Turn turn) {
    auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    if (found != events.end() && found->beat == selectedBeat_) {
        if (found->direction == turn) return;
        SaveUndo();
        found->direction = turn;
    } else {
        SaveUndo();
        events.insert(found, TurnEvent{selectedBeat_, turn});
    }
    MarkDirty();
}

bool ChartEditor::RemoveTurn() {
    auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    if (found == events.end() || found->beat != selectedBeat_) return false;
    SaveUndo();
    events.erase(found);
    MarkDirty();
    return true;
}

bool ChartEditor::MoveTurn(int beats) {
    if (beats == 0 || !SelectedTurn()) return false;
    const long long destination = static_cast<long long>(selectedBeat_) + beats;
    if (destination < chart_.route.firstBeat || destination >= chart_.route.lengthBeats) return false;
    auto& events = chart_.route.turns;
    if (std::any_of(events.begin(), events.end(),
                    [destination](const TurnEvent& event) {
                        return event.beat == destination;
                    })) return false;
    const Turn direction = SelectedTurn()->direction;
    SaveUndo();
    const auto original = std::lower_bound(events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    events.erase(original);
    const auto newSlot = std::lower_bound(events.begin(), events.end(),
        static_cast<int>(destination),
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    events.insert(newSlot, TurnEvent{static_cast<int>(destination), direction});
    selectedBeat_ = static_cast<int>(destination);
    MarkDirty();
    return true;
}

ChartEditor::Pattern ChartEditor::CopyTurns(int firstBeat, int lastBeat) const {
    Pattern result;
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats) return result;
    result.span = lastBeat - firstBeat + 1;
    for (const auto& event : chart_.route.turns) {
        if (event.beat < firstBeat) continue;
        if (event.beat > lastBeat) break;
        result.notes.push_back({event.beat - firstBeat, event.direction});
    }
    return result;
}

bool ChartEditor::PasteTurns(int destinationBeat, const Pattern& pattern) {
    if (pattern.span < 1 || pattern.notes.empty() ||
        destinationBeat < chart_.route.firstBeat ||
        static_cast<long long>(destinationBeat) + pattern.span >
            chart_.route.lengthBeats) return false;
    std::vector<TurnEvent> updated = chart_.route.turns;
    int lastRelative = -1;
    bool changed = false;
    for (const auto& note : pattern.notes) {
        const long long absolute = static_cast<long long>(destinationBeat) + note.beat;
        if (note.beat < 0 || note.beat >= pattern.span ||
            note.beat <= lastRelative || absolute >= chart_.route.lengthBeats)
            return false; // Validate whole clipboard; never partially paste.
        lastRelative = note.beat;
        const int beat = static_cast<int>(absolute);
        const auto found = std::lower_bound(updated.begin(), updated.end(), beat,
            [](const TurnEvent& event, int n) { return event.beat < n; });
        if (found != updated.end() && found->beat == beat) {
            if (found->direction != note.direction) {
                found->direction = note.direction;
                changed = true;
            }
        } else {
            updated.insert(found, {beat, note.direction});
            changed = true;
        }
    }
    if (!changed) return false;
    SaveUndo();
    chart_.route.turns = std::move(updated);
    MarkDirty();
    return true;
}

bool ChartEditor::ShiftTurns(int firstBeat, int lastBeat, int deltaBeats) {
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats || deltaBeats == 0) return false;
    std::vector<TurnEvent> updated = chart_.route.turns;
    bool moved = false;
    for (auto& event : updated) {
        if (event.beat < firstBeat || event.beat > lastBeat) continue;
        const long long destination = static_cast<long long>(event.beat) + deltaBeats;
        if (destination < chart_.route.firstBeat || destination >= chart_.route.lengthBeats) return false;
        event.beat = static_cast<int>(destination);
        moved = true;
    }
    if (!moved) return false;
    std::sort(updated.begin(), updated.end(),
              [](const TurnEvent& a, const TurnEvent& b) { return a.beat < b.beat; });
    for (std::size_t i = 1; i < updated.size(); ++i)
        if (updated[i - 1].beat == updated[i].beat) return false; // No collision.
    SaveUndo();
    chart_.route.turns = std::move(updated);
    MarkDirty();
    return true;
}

bool ChartEditor::InvertTurns(int firstBeat, int lastBeat) {
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats) return false;
    bool any = false;
    for (const auto& event : chart_.route.turns)
        if (event.beat >= firstBeat && event.beat <= lastBeat) { any = true; break; }
    if (!any) return false;
    SaveUndo();
    for (auto& event : chart_.route.turns) {
        if (event.beat >= firstBeat && event.beat <= lastBeat)
            event.direction = event.direction == Turn::Left ? Turn::Right : Turn::Left;
    }
    MarkDirty();
    return true;
}

bool ChartEditor::DeleteTurns(int firstBeat, int lastBeat) {
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats) return false;
    auto& events = chart_.route.turns;
    const auto start = std::lower_bound(events.begin(), events.end(), firstBeat,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    const auto finish = std::upper_bound(events.begin(), events.end(), lastBeat,
        [](int beat, const TurnEvent& event) { return beat < event.beat; });
    if (start == finish) return false;
    SaveUndo();
    events.erase(start, finish);
    MarkDirty();
    return true;
}

} // namespace locus::core
