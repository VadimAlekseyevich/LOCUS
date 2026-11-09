#include "core/chart_editor.h"
#include "core/utf8.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <utility>

namespace locus::core {

ChartEditor::ChartEditor() {
    NewDraft();
}

const ChartDifficulty& ChartEditor::Difficulty() const {
    if (difficultyRevision_ != revision_) {
        difficulty_ = CalculateChartDifficulty(chart_.route,
                                                chart_.offsetSeconds);
        difficultyRevision_ = revision_;
    }
    return difficulty_;
}

void ChartEditor::NewDraft() {
    chart_ = LoadedChart{};
    chart_.title = "Untitled Chart";
    chart_.audioPath = "song.mp3";
    chart_.route = Chart{};
    chart_.route.bpm = 120.0;
    chart_.route.lengthBeats = 32;
    songDurationSeconds_ = 0.0;
    chart_.route.start = Cell{3, 3}; // Automatic centre of default 8x8 grid.
    chart_.route.initialHeading = Heading::Right;
    chart_.route.turns.clear();
    chart_.offsetSeconds = 0.0;
    selectedBeat_ = 0;
    ResetHistory();
}

void ChartEditor::Load(const LoadedChart& source) {
    chart_ = source;
    // Older files may have rectangular editor windows. The current UI
    // exposes one square size; leave their route events and start intact
    // while normalising their visible viewport to the larger dimension.
    const int square = std::max(chart_.route.gridWidth, chart_.route.gridHeight);
    chart_.route.gridWidth = square;
    chart_.route.gridHeight = square;
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
    if (!ComputeSongBeatRange(chart_.route.bpm * chart_.route.ticksPerBeat,
                              chart_.offsetSeconds, songDurationSeconds_, first, end, error)) return false;
    if (!TurnRangeFits(first, end)) return false;
    chart_.route.firstBeat = first;
    chart_.route.lengthBeats = end;
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
    difficultyRevision_ = std::numeric_limits<std::uint64_t>::max();
}

void ChartEditor::MarkSaved() noexcept {
    savedRevision_ = revision_;
    dirty_ = false;
}

void ChartEditor::ApplySavedChart(LoadedChart saved) {
    // Save-to-disk has already validated/reopened the file. Replacing only the
    // serialized snapshot avoids Load() resetting the user's current beat
    // (and erasing the Undo/Redo history) on every Ctrl+S.
    chart_ = std::move(saved);
    difficultyRevision_ = std::numeric_limits<std::uint64_t>::max();
    selectedBeat_ = std::clamp(selectedBeat_, chart_.route.firstBeat,
                               chart_.route.lengthBeats - 1);
    MarkSaved();
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
           static_cast<double>(beat) * 60.0 /
               (chart_.route.bpm * chart_.route.ticksPerBeat);
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

bool ChartEditor::StepSnapSelection(int direction) noexcept {
    if (direction == 0) return false;
    const int step = chart_.route.ticksPerBeat / chart_.route.snapDivision;
    const double base = static_cast<double>(selectedBeat_) / step;
    const double next = (direction > 0 ? std::floor(base) + 1.0
                                       : std::ceil(base) - 1.0) * step;
    return SelectBeat(static_cast<int>(std::clamp(
        static_cast<long long>(next),
        static_cast<long long>(chart_.route.firstBeat),
        static_cast<long long>(chart_.route.lengthBeats - 1))));
}

bool ChartEditor::SetSnapDivision(int division) {
    constexpr int kAllowed[] = {1, 2, 3, 4, 6, 8, 12, 16, 24};
    if (std::find(std::begin(kAllowed), std::end(kAllowed), division) ==
        std::end(kAllowed)) return false;
    if (division == chart_.route.snapDivision) return true;

    const int oldTicks = chart_.route.ticksPerBeat;
    const int newTicks = std::lcm(oldTicks, division);
    if (newTicks > 48) return false;
    const int factor = newTicks / oldTicks;
    int first = chart_.route.firstBeat;
    int end = chart_.route.lengthBeats;
    if (factor > 1) {
        if (HasSongDuration()) {
            std::string error;
            if (!ComputeSongBeatRange(chart_.route.bpm * newTicks,
                                      chart_.offsetSeconds, songDurationSeconds_,
                                      first, end, error)) return false;
        } else {
            if (static_cast<long long>(first) * factor < -1000000 ||
                static_cast<long long>(end) * factor > 1000000 ||
                static_cast<long long>(end - first) * factor > 1000000)
                return false;
            first *= factor;
            end *= factor;
        }
        for (const auto& event : chart_.route.turns) {
            const long long tick = static_cast<long long>(event.beat) * factor;
            if (tick < first || tick >= end || tick < -1000000 || tick > 999999)
                return false;
        }
    }

    SaveUndo();
    for (auto& event : chart_.route.turns) event.beat *= factor;
    selectedBeat_ = std::clamp(selectedBeat_ * factor, first, end - 1);
    chart_.route.firstBeat = first;
    chart_.route.lengthBeats = end;
    chart_.route.ticksPerBeat = newTicks;
    chart_.route.snapDivision = division;
    MarkDirty();
    return true;
}

bool ChartEditor::SetBpm(double bpm) {
    if (!std::isfinite(bpm) || bpm <= 0.0) return false;
    if (bpm == chart_.route.bpm) return true;
    int first = chart_.route.firstBeat, end = chart_.route.lengthBeats;
    std::string error;
    if (HasSongDuration() &&
        !ComputeSongBeatRange(bpm * chart_.route.ticksPerBeat,
                              chart_.offsetSeconds, songDurationSeconds_,
                              first, end, error)) return false;

    // BPM changes only the ruler and snap coordinates, never a note's
    // absolute millisecond timestamp. Commit atomically after validation.
    std::vector<TurnEvent> remapped = chart_.route.turns;
    int prior = first - 1;
    for (auto& note : remapped) {
        const auto ms = EventTimeMs(chart_.route, note, chart_.offsetSeconds);
        note.beat = TickAtTimeMs(bpm, chart_.route.ticksPerBeat,
                                 chart_.offsetSeconds, ms);
        if (note.beat <= prior || note.beat < first || note.beat >= end)
            return false;
        note.timeMs = ms;
        prior = note.beat;
    }
    const std::int64_t selectedMs = static_cast<std::int64_t>(std::llround(
        AudioSecondsAtBeat(selectedBeat_) * 1000.0));
    SaveUndo();
    chart_.route.turns = std::move(remapped);
    chart_.route.bpm = bpm;
    chart_.route.firstBeat = first;
    chart_.route.lengthBeats = end;
    selectedBeat_ = std::clamp(
        TickAtTimeMs(bpm, chart_.route.ticksPerBeat, chart_.offsetSeconds,
                     selectedMs), first, end - 1);
    MarkDirty();
    return true;
}

bool ChartEditor::SetOffsetMs(double milliseconds) {
    if (!std::isfinite(milliseconds) ||
        std::abs(milliseconds) > 3600000.0 ||
        std::trunc(milliseconds) != milliseconds) return false;
    const double offset = milliseconds / 1000.0;
    if (offset == chart_.offsetSeconds) return true;
    int first = chart_.route.firstBeat, end = chart_.route.lengthBeats;
    std::string error;
    if (HasSongDuration() &&
        !ComputeSongBeatRange(chart_.route.bpm * chart_.route.ticksPerBeat,
                              offset, songDurationSeconds_,
                              first, end, error)) return false;

    // Changing the grid origin must not move authored audio timestamps.
    std::vector<TurnEvent> remapped = chart_.route.turns;
    int prior = first - 1;
    for (auto& note : remapped) {
        const auto ms = EventTimeMs(chart_.route, note, chart_.offsetSeconds);
        note.beat = TickAtTimeMs(chart_.route.bpm, chart_.route.ticksPerBeat,
                                 offset, ms);
        if (note.beat <= prior || note.beat < first || note.beat >= end)
            return false;
        note.timeMs = ms;
        prior = note.beat;
    }
    const std::int64_t selectedMs = static_cast<std::int64_t>(std::llround(
        AudioSecondsAtBeat(selectedBeat_) * 1000.0));
    SaveUndo();
    chart_.route.turns = std::move(remapped);
    chart_.offsetSeconds = offset;
    chart_.route.firstBeat = first;
    chart_.route.lengthBeats = end;
    selectedBeat_ = std::clamp(
        TickAtTimeMs(chart_.route.bpm, chart_.route.ticksPerBeat, offset,
                     selectedMs), first, end - 1);
    MarkDirty();
    return true;
}

bool ChartEditor::SetTitle(const std::string& title) {
    if (!utf8::ValidName(title, 100, 1024)) return false;
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
    // This is now one square visibility size, not two independent axes.
    if (width != height || width < kMinGridSize || width > kMaxGridSize)
        return false;
    const Cell automaticStart{(width - 1) / 2, (width - 1) / 2};
    if (width != chart_.route.gridWidth ||
        height != chart_.route.gridHeight ||
        chart_.route.start.x != automaticStart.x ||
        chart_.route.start.y != automaticStart.y) {
        SaveUndo();
        chart_.route.gridWidth = width;
        chart_.route.gridHeight = height;
        chart_.route.start = automaticStart;
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
    RotateHeading45(quarterTurns * 2);
}

void ChartEditor::RotateHeading45(int eighthTurns) {
    const int steps = (eighthTurns % 8 + 8) % 8;
    if (steps == 0) return;
    SaveUndo();
    chart_.route.initialHeading =
        locus::core::RotateHeading(chart_.route.initialHeading, steps);
    MarkDirty();
}

bool ChartEditor::SetOutgoingHeading(Heading heading) {
    if (!ValidHeading(heading)) return false;
    const Heading incoming = IncomingHeadingAt(chart_.route, selectedBeat_);
    if (heading == incoming) return RemoveTurn(); // Straight means no note.
    if (SelectedTurn() && SelectedTurn()->direction == heading) return false;
    PutTurn(heading);
    return true;
}

const TurnEvent* ChartEditor::SelectedTurn() const noexcept {
    const auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    return (found != events.end() && found->beat == selectedBeat_) ? &*found : nullptr;
}

void ChartEditor::PutTurn(Heading heading) {
    auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    if (found != events.end() && found->beat == selectedBeat_) {
        if (found->direction == heading) return;
        SaveUndo();
        found->direction = heading;
        found->timeMs = static_cast<std::int64_t>(
            std::llround(AudioSecondsAtBeat(selectedBeat_) * 1000.0));
    } else {
        SaveUndo();
        events.insert(found, TurnEvent{
            selectedBeat_, heading, static_cast<std::int64_t>(
                std::llround(AudioSecondsAtBeat(selectedBeat_) * 1000.0))});
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
    const Heading direction = SelectedTurn()->direction;
    SaveUndo();
    const auto original = std::lower_bound(events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    events.erase(original);
    const auto newSlot = std::lower_bound(events.begin(), events.end(),
        static_cast<int>(destination),
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    events.insert(newSlot, TurnEvent{
        static_cast<int>(destination), direction,
        static_cast<std::int64_t>(std::llround(
            AudioSecondsAtBeat(static_cast<int>(destination)) * 1000.0))});
    selectedBeat_ = static_cast<int>(destination);
    MarkDirty();
    return true;
}

ChartEditor::Pattern ChartEditor::CopyTurns(int firstBeat, int lastBeat) const {
    Pattern result;
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats) return result;

    // Empty ticks outside the selected notes should never offset a paste.
    // Keep all the spacing BETWEEN notes by anchoring to the first real event
    // and ending at the last real event, regardless of drag direction.
    const auto start = std::lower_bound(chart_.route.turns.begin(),
                                       chart_.route.turns.end(), firstBeat,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    const auto end = std::upper_bound(start, chart_.route.turns.end(), lastBeat,
        [](int beat, const TurnEvent& event) { return beat < event.beat; });
    if (start == end) return result; // No note => empty clipboard.
    const int firstNote = start->beat;
    const int lastNote = (end - 1)->beat;
    result.span = lastNote - firstNote + 1;
    result.ticksPerBeat = chart_.route.ticksPerBeat;
    for (auto note = start; note != end; ++note)
        result.notes.push_back({note->beat - firstNote, note->direction});
    return result;
}

bool ChartEditor::PasteTurns(int destinationBeat, const Pattern& pattern) {
    // Clipboard positions are expressed in its source tick resolution.
    // A finer placement grid must not stretch a copied musical phrase.
    if (pattern.span < 1 || pattern.notes.empty() ||
        pattern.ticksPerBeat < 1 ||
        chart_.route.ticksPerBeat % pattern.ticksPerBeat != 0)
        return false;
    const int factor = chart_.route.ticksPerBeat / pattern.ticksPerBeat;
    const long long span = static_cast<long long>(pattern.span - 1) * factor + 1;
    if (destinationBeat < chart_.route.firstBeat ||
        static_cast<long long>(destinationBeat) + span > chart_.route.lengthBeats)
        return false;
    std::vector<TurnEvent> updated = chart_.route.turns;
    int lastRelative = -1;
    bool changed = false;
    for (const auto& note : pattern.notes) {
        if (note.beat < 0 || note.beat >= pattern.span ||
            note.beat <= lastRelative) return false;
        lastRelative = note.beat;
        const long long absolute =
            static_cast<long long>(destinationBeat) +
            static_cast<long long>(note.beat) * factor;
        if (absolute < chart_.route.firstBeat ||
            absolute >= chart_.route.lengthBeats) return false;
        const int beat = static_cast<int>(absolute);
        const auto found = std::lower_bound(updated.begin(), updated.end(), beat,
            [](const TurnEvent& event, int n) { return event.beat < n; });
        const std::int64_t ms = static_cast<std::int64_t>(
            std::llround(AudioSecondsAtBeat(beat) * 1000.0));
        if (found != updated.end() && found->beat == beat) {
            if (found->direction != note.direction || found->timeMs != ms) {
                found->direction = note.direction;
                found->timeMs = ms;
                changed = true;
            }
        } else {
            updated.insert(found, {beat, note.direction, ms});
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
        event.timeMs = static_cast<std::int64_t>(
            std::llround(AudioSecondsAtBeat(event.beat) * 1000.0));
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

bool ChartEditor::MirrorTurnsHorizontal(int firstBeat, int lastBeat) {
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats) return false;
    auto& turns = chart_.route.turns;
    const auto first = std::lower_bound(turns.begin(), turns.end(), firstBeat,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    const auto last = std::upper_bound(first, turns.end(), lastBeat,
        [](int beat, const TurnEvent& event) { return beat < event.beat; });
    if (first == last) return false;
    bool changed = std::any_of(first, last, [](const TurnEvent& event) {
        return MirrorHeadingHorizontal(event.direction) != event.direction;
    });
    if (!changed) return false;
    SaveUndo();
    for (auto it = first; it != last; ++it)
        it->direction = MirrorHeadingHorizontal(it->direction);
    MarkDirty();
    return true;
}

bool ChartEditor::MirrorTurnsVertical(int firstBeat, int lastBeat) {
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats) return false;
    auto& turns = chart_.route.turns;
    const auto first = std::lower_bound(turns.begin(), turns.end(), firstBeat,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    const auto last = std::upper_bound(first, turns.end(), lastBeat,
        [](int beat, const TurnEvent& event) { return beat < event.beat; });
    if (first == last) return false;
    bool changed = std::any_of(first, last, [](const TurnEvent& event) {
        return MirrorHeadingVertical(event.direction) != event.direction;
    });
    if (!changed) return false;
    SaveUndo();
    for (auto it = first; it != last; ++it)
        it->direction = MirrorHeadingVertical(it->direction);
    MarkDirty();
    return true;
}

bool ChartEditor::RotateTurns(int firstBeat, int lastBeat, int quarterTurns) {
    const int steps = (quarterTurns % 4 + 4) % 4;
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats || steps == 0) return false;
    auto& turns = chart_.route.turns;
    const auto first = std::lower_bound(turns.begin(), turns.end(), firstBeat,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    const auto last = std::upper_bound(first, turns.end(), lastBeat,
        [](int beat, const TurnEvent& event) { return beat < event.beat; });
    if (first == last) return false;

    SaveUndo();
    for (auto it = first; it != last; ++it)
        it->direction = locus::core::RotateHeading(it->direction, steps * 2);
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
