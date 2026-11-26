#include "core/chart_editor.h"
#include "core/tempo_map.h"
#include "core/utf8.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iterator>
#include <limits>
#include <numeric>
#include <utility>

namespace locus::core {

ChartEditor::ChartEditor() {
    NewDraft();
}

std::array<int,8> CountDirectionPresses(const Chart& chart) noexcept {
    std::array<int,8> counts{};
    for(const auto& note:chart.turns) {
        if(!ValidHeading(note.direction)) continue;
        const auto mask=HeadingArrowMask(note.direction);
        for(int cardinal=0;cardinal<4;++cardinal)
            if(mask&(1u<<cardinal)) ++counts[static_cast<std::size_t>(cardinal)];
        if(IsDiagonal(note.direction))
            ++counts[static_cast<std::size_t>(note.direction)];
    }
    return counts;
}
const std::array<int,8>& ChartEditor::DirectionPressCounts() const noexcept {
    if(directionCountsRevision_!=RouteRevision()) {
        directionCounts_=CountDirectionPresses(chart_.route);
        directionCountsRevision_=RouteRevision();
    }
    return directionCounts_;
}

const ChartDifficulty& ChartEditor::Difficulty() const {
    if (difficultyRevision_ != routeRevision_) {
        difficulty_ = CalculateChartDifficulty(chart_.route,
            chart_.offsetSeconds,&difficultyGraph_,SongDurationSeconds());
        difficultyRevision_ = routeRevision_;
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
    selectedTurnTimeMs_.reset();
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
    selectedTurnTimeMs_.reset();
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
    if (old != seconds) ++routeRevision_;
    return true;
}

bool ChartEditor::ReflowSongRange() noexcept {
    if (songDurationSeconds_ <= 0.0) return true; // Temporary unbound draft.
    int first = 0, end = 0;
    std::string error;
    if (!TempoTickRange(chart_.route, chart_.offsetSeconds,
                        songDurationSeconds_, first, end, error)) return false;
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
    ++routeRevision_;
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

void ChartEditor::BeginUnsavedDocument() noexcept {
    // Keep current chart/tempo metadata, but don't let Ctrl+Z revert the
    // Song Set template (or silently make an unsaved new chart clean).
    ResetHistory();
    savedRevision_ = std::numeric_limits<std::uint64_t>::max();
    dirty_ = true;
}

void ChartEditor::ApplySavedChart(LoadedChart saved) {
    // Save-to-disk has already validated/reopened the file. Replacing only the
    // serialized snapshot avoids Load() resetting the user's current beat
    // (and erasing the Undo/Redo history) on every Ctrl+S.
    chart_ = std::move(saved);
    ++routeRevision_;
    difficultyRevision_ = std::numeric_limits<std::uint64_t>::max();
    selectedBeat_ = std::clamp(selectedBeat_, chart_.route.firstBeat,
                               chart_.route.lengthBeats - 1);
    MarkSaved();
}

ChartEditor::HistoryMetadata ChartEditor::CaptureMetadata() const {
    const auto& route = chart_.route;
    HistoryMetadata state;
    state.title = chart_.title;
    state.audioPath = chart_.audioPath;
    state.offsetSeconds = chart_.offsetSeconds;
    state.previewLeadBeats = chart_.previewLeadBeats;
    state.bpm = route.bpm;
    state.tempoMap = route.tempoMap;
    state.ticksPerBeat = route.ticksPerBeat;
    state.snapDivision = route.snapDivision;
    state.gridWidth = route.gridWidth;
    state.gridHeight = route.gridHeight;
    state.start = route.start;
    state.initialHeading = route.initialHeading;
    state.firstBeat = route.firstBeat;
    state.lengthBeats = route.lengthBeats;
    state.selectedBeat = selectedBeat_;
    state.selectedTurnTimeMs = selectedTurnTimeMs_;
    state.revision = revision_;
    return state;
}

void ChartEditor::RestoreMetadata(const HistoryMetadata& state) {
    ++routeRevision_;
    auto& route = chart_.route;
    chart_.title = state.title;
    chart_.audioPath = state.audioPath;
    chart_.offsetSeconds = state.offsetSeconds;
    chart_.previewLeadBeats = state.previewLeadBeats;
    route.bpm = state.bpm;
    route.tempoMap = state.tempoMap;
    route.ticksPerBeat = state.ticksPerBeat;
    route.snapDivision = state.snapDivision;
    route.gridWidth = state.gridWidth;
    route.gridHeight = state.gridHeight;
    route.start = state.start;
    route.initialHeading = state.initialHeading;
    route.firstBeat = state.firstBeat;
    route.lengthBeats = state.lengthBeats;
    selectedBeat_ = state.selectedBeat;
    selectedTurnTimeMs_ = state.selectedTurnTimeMs;
    revision_ = state.revision;
    dirty_ = revision_ != savedRevision_;
    difficultyRevision_ = std::numeric_limits<std::uint64_t>::max();
}

void ChartEditor::SaveUndo() {
    SaveUndo(1, 0); // no turns changed
}

void ChartEditor::SaveUndoAllTurns() {
    SaveUndo(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
}

void ChartEditor::SaveUndo(int firstTouchedBeat, int lastTouchedBeat) {
    HistoryEdit edit;
    edit.before = CaptureMetadata();
    edit.firstTouchedBeat = firstTouchedBeat;
    edit.lastTouchedBeat = lastTouchedBeat;
    const auto& turns = chart_.route.turns;
    if (firstTouchedBeat <= lastTouchedBeat) {
        const auto begin = std::lower_bound(turns.begin(), turns.end(),
            firstTouchedBeat,
            [](const TurnEvent& event, int beat) { return event.beat < beat; });
        const auto end = std::upper_bound(begin, turns.end(), lastTouchedBeat,
            [](int beat, const TurnEvent& event) { return beat < event.beat; });
        edit.turnIndex = static_cast<std::size_t>(begin - turns.begin());
        edit.beforeTurns.assign(begin, end);
    }
    if (undo_.size() == kMaxHistory) undo_.pop_front();
    undo_.push_back(std::move(edit));
    redo_.clear();
}

void ChartEditor::MarkDirty() {
    ++routeRevision_;
    revision_ = ++nextRevision_;
    dirty_ = revision_ != savedRevision_;
    // Every mutator calls SaveUndo before its first modification, then
    // MarkDirty exactly once. Complete the reversible transaction here.
    HistoryEdit& edit = undo_.back();
    edit.after = CaptureMetadata();
    if (edit.firstTouchedBeat <= edit.lastTouchedBeat) {
        const auto& turns = chart_.route.turns;
        const auto begin = std::lower_bound(turns.begin(), turns.end(),
            edit.firstTouchedBeat,
            [](const TurnEvent& event, int beat) { return event.beat < beat; });
        const auto end = std::upper_bound(begin, turns.end(), edit.lastTouchedBeat,
            [](int beat, const TurnEvent& event) { return beat < event.beat; });
        // A reversible range must contain every changed note. If this
        // invariant breaks, the beginning index may shift and Undo would
        // otherwise replace a neighboring note.
        assert(static_cast<std::size_t>(begin - turns.begin()) == edit.turnIndex);
        edit.afterTurns.assign(begin, end);
    }
}

void ChartEditor::ApplyHistory(const HistoryEdit& edit, bool undo) {
    const auto& outgoing = undo ? edit.afterTurns : edit.beforeTurns;
    const auto& incoming = undo ? edit.beforeTurns : edit.afterTurns;
    if (edit.firstTouchedBeat <= edit.lastTouchedBeat) {
        auto& turns = chart_.route.turns;
        const auto start = turns.begin() +
            static_cast<std::ptrdiff_t>(edit.turnIndex);
        const auto finish = start + static_cast<std::ptrdiff_t>(outgoing.size());
        turns.erase(start, finish);
        turns.insert(turns.begin() + static_cast<std::ptrdiff_t>(edit.turnIndex),
                     incoming.begin(), incoming.end());
    }
    RestoreMetadata(undo ? edit.before : edit.after);
}

bool ChartEditor::Undo() {
    if (undo_.empty()) return false;
    HistoryEdit edit = std::move(undo_.back());
    undo_.pop_back();
    ApplyHistory(edit, true);
    redo_.push_back(std::move(edit));
    return true;
}

bool ChartEditor::Redo() {
    if (redo_.empty()) return false;
    HistoryEdit edit = std::move(redo_.back());
    redo_.pop_back();
    ApplyHistory(edit, false);
    undo_.push_back(std::move(edit));
    return true;
}

std::size_t ChartEditor::HistoryStoredTurns() const noexcept {
    std::size_t count = 0;
    for (const auto& edit : undo_)
        count += edit.beforeTurns.size() + edit.afterTurns.size();
    for (const auto& edit : redo_)
        count += edit.beforeTurns.size() + edit.afterTurns.size();
    return count;
}

double ChartEditor::AudioSecondsAtBeat(int beat) const noexcept {
    return TempoSecondsAtTick(chart_.route, chart_.offsetSeconds, beat);
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
        beat >= chart_.route.lengthBeats) return false;
    const bool changed = selectedBeat_ != beat || selectedTurnTimeMs_.has_value();
    selectedBeat_ = beat;
    selectedTurnTimeMs_.reset();
    return changed;
}

bool ChartEditor::SelectTurnAtTimeMs(std::int64_t timeMs) noexcept {
    const auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), timeMs,
        [&](const TurnEvent& event, std::int64_t ms) {
            return EventTimeMs(chart_.route, event, chart_.offsetSeconds) < ms;
        });
    if (found == events.end() ||
        EventTimeMs(chart_.route, *found, chart_.offsetSeconds) != timeMs)
        return false;
    const bool changed = selectedBeat_ != found->beat ||
                         selectedTurnTimeMs_ != timeMs;
    selectedBeat_ = found->beat;
    selectedTurnTimeMs_ = timeMs;
    return changed;
}

bool ChartEditor::StepSelection(int deltaBeats) noexcept {
    // Avoid out-of-range selections at either edge of a short timeline.
    const long long next = std::clamp(
        static_cast<long long>(selectedBeat_) + deltaBeats,
        static_cast<long long>(chart_.route.firstBeat),
        static_cast<long long>(chart_.route.lengthBeats - 1));
    return SelectBeat(static_cast<int>(next));
}

bool ChartEditor::StepSnapSelection(int direction) {
    if (direction == 0) return false;
    const auto& chart = chart_.route;
    const int step = chart.ticksPerBeat / chart.snapDivision;
    const double base = static_cast<double>(selectedBeat_) / step;
    const long long first = chart.firstBeat;
    const long long end = chart.lengthBeats - 1;
    long long next = static_cast<long long>(
        (direction > 0 ? std::floor(base) + 1.0
                       : std::ceil(base) - 1.0) * step);
    for (; next >= first && next <= end; next += direction > 0 ? step : -step) {
        // A tempo marker may begin between the old section's subdivisions.
        // Such zero-duration grid indices are reserved, not authorable.
        if (chart.tempoMap.size() > 1) {
            const double seconds = TempoSecondsAtTick(
                chart, chart_.offsetSeconds, static_cast<double>(next));
            if (TempoSnapTickAtSeconds(chart, chart_.offsetSeconds, seconds)
                != next) continue;
        }
        return SelectBeat(static_cast<int>(next));
    }
    return SelectBeat(static_cast<int>(direction > 0 ? end : first));
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
    std::vector<TurnEvent> remapped;
    int selected=selectedBeat_*factor;
    if (factor > 1) {
        Chart projected=chart_.route;
        projected.ticksPerBeat=newTicks;
        if (HasSongDuration()) {
            std::string error;
            if (!TempoTickRange(projected, chart_.offsetSeconds,
                                songDurationSeconds_, first, end, error)) return false;
        } else {
            if (static_cast<long long>(first) * factor < -1000000 ||
                static_cast<long long>(end) * factor > 1000000 ||
                static_cast<long long>(end - first) * factor > 1000000)
                return false;
            first *= factor;
            end *= factor;
        }
        // The previous tick is a rounded projection, not the authored
        // timestamp. Reproject exact notes directly onto the finer lattice.
        // Tick-only internal notes retain their exact musical grid location.
        const TempoProjection projection(projected,chart_.offsetSeconds);
        int previous=first;
        if (selectedTurnTimeMs_)
            selected=projection.TickAtMs(*selectedTurnTimeMs_);
        for (auto& event:projected.turns) {
            const long long tick=event.timeMs==kNoTimeMs
                ? static_cast<long long>(event.beat)*factor
                : projection.TickAtMs(event.timeMs);
            if (tick<previous || tick<first || tick>=end ||
                tick<-1000000 || tick>999999) return false;
            if (selectedTurnTimeMs_ &&
                EventTimeMs(chart_.route,event,chart_.offsetSeconds)==*selectedTurnTimeMs_)
                selected=static_cast<int>(tick);
            event.beat=static_cast<int>(tick);
            previous=event.beat;
        }
        remapped=std::move(projected.turns);
    }

    if (factor>1) {
        SaveUndoAllTurns();
        chart_.route.turns=std::move(remapped);
    } else SaveUndo();
    selectedBeat_=std::clamp(selected,first,end-1);
    chart_.route.firstBeat = first;
    chart_.route.lengthBeats = end;
    chart_.route.ticksPerBeat = newTicks;
    chart_.route.snapDivision = division;
    MarkDirty();
    return true;
}

bool ChartEditor::SetTempoMap(const std::vector<TempoMarker>& markers) {
    std::string error;
    if (!ValidTempoMap(markers, error)) return false;
    const auto existing = EffectiveTempoMap(
        chart_.route, chart_.offsetSeconds);
    if (existing == markers) return true;

    // A tempo change alters only the beat lattice. Notes remain identified
    // by their absolute millisecond timestamps, even when two project into
    // one grid tick. Do NOT upsample to invent uniqueness on the grid.
    const double selectedTime = selectedTurnTimeMs_
        ? *selectedTurnTimeMs_ / 1000.0 : AudioSecondsAtBeat(selectedBeat_);
    Chart adjusted = chart_.route;
    adjusted.bpm = markers.front().bpm;
    adjusted.tempoMap = markers;
    const double nextOffset = markers.front().timeMs / 1000.0;
    int first = adjusted.firstBeat, end = adjusted.lengthBeats;
    if (HasSongDuration() &&
        !TempoTickRange(adjusted, nextOffset,
                        songDurationSeconds_, first, end, error))
        return false;

    const TempoProjection projection(adjusted,nextOffset);
    // Reuse the transaction's existing note copy instead of copying it twice.
    int previous = first;
    for (auto& turn : adjusted.turns) {
        const std::int64_t ms = EventTimeMs(
            chart_.route, turn, chart_.offsetSeconds);
        const int tick = projection.TickAtMs(ms);
        if (tick < previous || tick < first || tick >= end ||
            tick < -1000000 || tick > 999999)
            return false;
        turn.beat = tick;
        turn.timeMs = ms;
        previous = tick;
    }
    const int selected = std::clamp(
        projection.TickAtMs(
            static_cast<std::int64_t>(std::llround(selectedTime * 1000.0))),
        first, end - 1);

    SaveUndoAllTurns();
    chart_.route.turns = std::move(adjusted.turns);
    chart_.route.tempoMap = markers;
    chart_.route.bpm = markers.front().bpm;
    chart_.offsetSeconds = nextOffset;
    chart_.route.firstBeat = first;
    chart_.route.lengthBeats = end;
    selectedBeat_ = selected;
    MarkDirty();
    return true;
}

bool ChartEditor::SetBpm(double bpm) {
    if (!std::isfinite(bpm) || bpm <= 0.0) return false;
    auto changes = EffectiveTempoMap(chart_.route, chart_.offsetSeconds);
    changes.front().bpm = bpm;
    return SetTempoMap(changes);
}

bool ChartEditor::SetOffsetMs(double milliseconds) {
    if (!std::isfinite(milliseconds) ||
        std::abs(milliseconds) > 3600000.0 ||
        std::trunc(milliseconds) != milliseconds) return false;
    auto changes = EffectiveTempoMap(chart_.route, chart_.offsetSeconds);
    changes.front().timeMs = static_cast<std::int64_t>(milliseconds);
    return SetTempoMap(changes);
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
    if (SelectedTurn() && SelectedTurn()->direction == heading) return false;
    PutTurn(heading);
    return true;
}

const TurnEvent* ChartEditor::SelectedTurn() const noexcept {
    const auto& events = chart_.route.turns;
    if (selectedTurnTimeMs_) {
        const auto exact = std::lower_bound(
            events.begin(), events.end(), *selectedTurnTimeMs_,
            [&](const TurnEvent& event, std::int64_t ms) {
                return EventTimeMs(chart_.route, event, chart_.offsetSeconds) < ms;
            });
        if (exact != events.end() && exact->beat == selectedBeat_ &&
            EventTimeMs(chart_.route, *exact, chart_.offsetSeconds) ==
                *selectedTurnTimeMs_)
            return &*exact;
        // Do not silently retarget a stale exact-time selection to another
        // event sharing its cached grid tick after an external reload.
        return nullptr;
    }
    const auto first = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    return first != events.end() && first->beat == selectedBeat_
        ? &*first : nullptr;
}

void ChartEditor::PutTurn(Heading heading) {
    auto& events = chart_.route.turns;
    auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    if (selectedTurnTimeMs_) {
        const auto* selected = SelectedTurn();
        if (!selected) return; // stale identity: never edit a neighbor
        found = events.begin() + (selected - events.data());
    }
    if (found != events.end() && found->beat == selectedBeat_) {
        if (found->direction == heading) return;
        SaveUndo(selectedBeat_, selectedBeat_);
        found->direction = heading;
        if (!selectedTurnTimeMs_) {
            found->timeMs = static_cast<std::int64_t>(
                std::llround(AudioSecondsAtBeat(selectedBeat_) * 1000.0));
        }
    } else {
        SaveUndo(selectedBeat_, selectedBeat_);
        events.insert(found, TurnEvent{
            selectedBeat_, heading, static_cast<std::int64_t>(
                std::llround(AudioSecondsAtBeat(selectedBeat_) * 1000.0))});
    }
    MarkDirty();
}

bool ChartEditor::PutTurnAtTimeMs(std::int64_t timeMs, Heading heading) {
    if (!ValidHeading(heading) || timeMs < -3600000 ||
        timeMs > 86400000 ||
        (HasSongDuration() && timeMs >= songDurationSeconds_ * 1000.0))
        return false;
    const auto& route = chart_.route;
    const int tick = TempoTickAtMs(route, chart_.offsetSeconds, timeMs);
    if (tick < route.firstBeat || tick >= route.lengthBeats ||
        (timeMs < 0 && !(chart_.offsetSeconds < 0.0 && tick >= 0)))
        return false;
    auto& notes = chart_.route.turns;
    const auto at = std::lower_bound(
        notes.begin(), notes.end(), timeMs,
        [&](const TurnEvent& event, std::int64_t ms) {
            return EventTimeMs(route, event, chart_.offsetSeconds) < ms;
        });
    if (at != notes.end() &&
        EventTimeMs(route, *at, chart_.offsetSeconds) == timeMs) {
        if (at->direction == heading) {
            SelectTurnAtTimeMs(timeMs);
            return false;
        }
        SaveUndo(tick, tick);
        at->direction = heading;
    } else {
        SaveUndo(tick, tick);
        notes.insert(at, {tick, heading, timeMs});
    }
    selectedBeat_ = tick;
    selectedTurnTimeMs_ = timeMs;
    MarkDirty();
    return true;
}

bool ChartEditor::RemoveTurn() {
    auto& events = chart_.route.turns;
    auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    if (selectedTurnTimeMs_) {
        const auto* selected = SelectedTurn();
        if (!selected) return false;
        found = events.begin() + (selected - events.data());
    }
    if (found == events.end() || found->beat != selectedBeat_) return false;
    SaveUndo(selectedBeat_, selectedBeat_);
    events.erase(found);
    selectedTurnTimeMs_.reset();
    MarkDirty();
    return true;
}

bool ChartEditor::MoveTurn(int beats) {
    const TurnEvent* selected = SelectedTurn();
    if (beats == 0 || !selected) return false;
    const long long target = static_cast<long long>(selectedBeat_) + beats;
    if (target < chart_.route.firstBeat || target >= chart_.route.lengthBeats)
        return false;
    const int destination = static_cast<int>(target);
    const auto& route = chart_.route;
    const auto sourceTime = EventTimeMs(
        route, *selected, chart_.offsetSeconds);
    const auto sourceTickTime = static_cast<std::int64_t>(
        std::llround(AudioSecondsAtBeat(selectedBeat_) * 1000.0));
    const auto targetTickTime = static_cast<std::int64_t>(
        std::llround(AudioSecondsAtBeat(destination) * 1000.0));
    // Moving an exact 1010ms note from tick 2 to tick 5 must give 2510ms,
    // not erase its 10ms microtiming. The destination tick still owns it.
    const std::int64_t timestamp =
        targetTickTime + (sourceTime - sourceTickTime);
    if (timestamp < -3600000 || timestamp > 86400000 ||
        (HasSongDuration() && timestamp >= songDurationSeconds_ * 1000.0) ||
        (timestamp < 0 &&
            !(chart_.offsetSeconds < 0.0 && destination >= 0)) ||
        TempoTickAtMs(route, chart_.offsetSeconds, timestamp) != destination)
        return false;

    auto& turns = chart_.route.turns;
    const std::size_t index = static_cast<std::size_t>(
        selected - turns.data());
    // Two different timestamps may live in one target cell, but the
    // absolute timestamp is unique. Never overwrite another event.
    for (std::size_t i = 0; i < turns.size(); ++i)
        if (i != index &&
            EventTimeMs(route, turns[i], chart_.offsetSeconds) == timestamp)
            return false;

    const Heading direction = selected->direction;
    SaveUndo(std::min(selectedBeat_, destination),
             std::max(selectedBeat_, destination));
    turns.erase(turns.begin() + static_cast<std::ptrdiff_t>(index));
    const auto slot = std::lower_bound(
        turns.begin(), turns.end(), timestamp,
        [&](const TurnEvent& note, std::int64_t timeMs) {
            return EventTimeMs(route, note, chart_.offsetSeconds) < timeMs;
        });
    turns.insert(slot, {destination, direction, timestamp});
    selectedBeat_ = destination;
    selectedTurnTimeMs_ = timestamp;
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
    result.notes.reserve(static_cast<std::size_t>(end-start));
    const TempoProjection projection(chart_.route,chart_.offsetSeconds);
    for (auto note = start; note != end; ++note) {
        const auto tickMs = static_cast<std::int64_t>(std::llround(
            projection.SecondsAtTick(note->beat) * 1000.0));
        const auto exactMs = EventTimeMs(
            chart_.route, *note, chart_.offsetSeconds);
        result.notes.push_back({
            note->beat - firstNote, note->direction, exactMs - tickMs
        });
    }
    return result;
}

bool ChartEditor::PasteTurns(int destinationBeat, const Pattern& pattern) {
    // Musical placement follows the destination grid; sub-tick offsets
    // preserve source-authored milliseconds. A pattern with multiple notes
    // in one cell must NEVER be flattened into one replacement.
    if (pattern.span < 1 || pattern.notes.empty() ||
        pattern.ticksPerBeat < 1 ||
        chart_.route.ticksPerBeat % pattern.ticksPerBeat != 0)
        return false;
    const int factor = chart_.route.ticksPerBeat / pattern.ticksPerBeat;
    const long long span = static_cast<long long>(pattern.span - 1) * factor + 1;
    if (destinationBeat < chart_.route.firstBeat ||
        static_cast<long long>(destinationBeat) + span > chart_.route.lengthBeats)
        return false;

    const auto& route = chart_.route;
    const TempoProjection projection(route,chart_.offsetSeconds);
    // Validate and project the complete clipboard before touching the draft.
    // Valid projected ticks are monotonic in song time; within one tick the
    // clipboard already requires strictly increasing offsets.
    std::vector<TurnEvent> incoming;
    incoming.reserve(pattern.notes.size());
    int lastRelative = -1;
    std::int64_t lastOffset = kNoTimeMs;
    for (const auto& note : pattern.notes) {
        if (!ValidHeading(note.direction) ||
            note.beat < 0 || note.beat >= pattern.span ||
            note.beat < lastRelative ||
            (note.beat == lastRelative &&
                (note.timeMs == kNoTimeMs || lastOffset == kNoTimeMs ||
                 note.timeMs <= lastOffset)))
            return false;
        lastRelative = note.beat;
        lastOffset = note.timeMs;
        const long long absolute =
            static_cast<long long>(destinationBeat) +
            static_cast<long long>(note.beat) * factor;
        if (absolute < route.firstBeat || absolute >= route.lengthBeats)
            return false;
        const int beat = static_cast<int>(absolute);
        const auto tickMs = static_cast<std::int64_t>(
            std::llround(projection.SecondsAtTick(beat) * 1000.0));
        // Sentinel means a traditional snapped note, offset zero.
        const auto intraTickMs =
            note.timeMs == kNoTimeMs ? 0 : note.timeMs;
        if (intraTickMs < -86400000 || intraTickMs > 86400000)
            return false;
        const std::int64_t ms = tickMs + intraTickMs;
        if (ms < -3600000 || ms > 86400000 ||
            (HasSongDuration() && ms >=
                songDurationSeconds_ * 1000.0) ||
            (ms < 0 && !(chart_.offsetSeconds < 0.0 && beat >= 0)) ||
            projection.TickAtMs(ms) != beat)
            return false;

        if (!incoming.empty() && ms <= incoming.back().timeMs)
            return false;
        incoming.push_back({beat,note.direction,ms});
    }

    // Merge only the affected tick interval. Existing and pasted notes
    // have sorted, unique timestamps. Equal timestamps replace direction
    // only, preserving the original note representation and identity.
    const int lastBeat=static_cast<int>(
        static_cast<long long>(destinationBeat)+span-1);
    const auto first=std::lower_bound(route.turns.begin(),route.turns.end(),
        destinationBeat,[](const TurnEvent& event,int tick) { return event.beat<tick; });
    const auto end=std::upper_bound(first,route.turns.end(),lastBeat,
        [](int tick,const TurnEvent& event) { return tick<event.beat; });
    const auto index=static_cast<std::size_t>(first-route.turns.begin());
    const auto replaced=static_cast<std::size_t>(end-first);
    std::vector<TurnEvent> merged;
    merged.reserve(replaced+incoming.size());
    auto old=first;
    auto add=incoming.cbegin();
    bool changed=false;
    while (old!=end && add!=incoming.cend()) {
        const auto oldMs=EventTimeMs(route,*old,chart_.offsetSeconds);
        if (oldMs<add->timeMs) {
            merged.push_back(*old++);
        } else if (oldMs>add->timeMs) {
            merged.push_back(*add++);
            changed=true;
        } else {
            if (old->beat!=add->beat) return false;
            auto event=*old++;
            changed |= event.direction!=add->direction;
            event.direction=add->direction;
            merged.push_back(event);
            ++add;
        }
    }
    merged.insert(merged.end(),old,end);
    changed |= add!=incoming.cend();
    merged.insert(merged.end(),add,incoming.cend());
    if (!changed) return false;
    // Protect the splice boundary even for an inconsistent in-memory
    // source whose projected tick does not match its authored timestamp.
    if (first!=route.turns.begin() &&
        EventTimeMs(route,*(first-1),chart_.offsetSeconds)>=
            EventTimeMs(route,merged.front(),chart_.offsetSeconds))
        return false;
    if (end!=route.turns.end() &&
        EventTimeMs(route,merged.back(),chart_.offsetSeconds)>=
            EventTimeMs(route,*end,chart_.offsetSeconds))
        return false;

    // Commit a single range splice and one history entry. Overwrite the
    // common prefix and shift the untouched suffix at most once (the
    // merged range never shrinks). No per-note insertion into the chart.
    auto& turns=chart_.route.turns;
    const auto extra=merged.size()-replaced;
    if (extra) turns.reserve(turns.size()+extra);
    SaveUndo(destinationBeat,lastBeat);
    auto target=turns.begin()+static_cast<std::ptrdiff_t>(index);
    std::copy_n(merged.begin(),replaced,target);
    if (extra) turns.insert(target+static_cast<std::ptrdiff_t>(replaced),
        merged.begin()+static_cast<std::ptrdiff_t>(replaced),merged.end());
    MarkDirty();
    return true;
}

bool ChartEditor::ShiftTurns(int firstBeat, int lastBeat, int deltaBeats) {
    if (firstBeat < chart_.route.firstBeat || lastBeat < firstBeat ||
        lastBeat >= chart_.route.lengthBeats || deltaBeats == 0)
        return false;
    const auto& route = chart_.route;
    const TempoProjection projection(route,chart_.offsetSeconds);
    std::vector<TurnEvent> updated = route.turns;
    bool moved = false;
    int touchedMin = std::numeric_limits<int>::max();
    int touchedMax = std::numeric_limits<int>::min();
    for (auto& event : updated) {
        if (event.beat < firstBeat || event.beat > lastBeat) continue;
        const long long target =
            static_cast<long long>(event.beat) + deltaBeats;
        if (target < route.firstBeat || target >= route.lengthBeats)
            return false;
        const int destination = static_cast<int>(target);
        const std::int64_t currentMs = EventTimeMs(
            route, event, chart_.offsetSeconds);
        const auto sourceTick = static_cast<std::int64_t>(
            std::llround(projection.SecondsAtTick(event.beat) * 1000.0));
        const auto targetTick = static_cast<std::int64_t>(
            std::llround(projection.SecondsAtTick(destination) * 1000.0));
        const std::int64_t nextMs =
            targetTick + (currentMs - sourceTick);
        if (nextMs < -3600000 || nextMs > 86400000 ||
            (HasSongDuration() && nextMs >= songDurationSeconds_ * 1000.0) ||
            (nextMs < 0 &&
                !(chart_.offsetSeconds < 0.0 && destination >= 0)) ||
            projection.TickAtMs(nextMs) != destination)
            return false;
        touchedMin = std::min({touchedMin, event.beat, destination});
        touchedMax = std::max({touchedMax, event.beat, destination});
        event.beat = destination;
        event.timeMs = nextMs;
        moved = true;
    }
    if (!moved) return false;
    std::sort(updated.begin(), updated.end(),
        [&](const TurnEvent& lhs, const TurnEvent& rhs) {
            return EventTimeMs(route, lhs, chart_.offsetSeconds) <
                   EventTimeMs(route, rhs, chart_.offsetSeconds);
        });
    for (std::size_t i = 1; i < updated.size(); ++i)
        if (EventTimeMs(route, updated[i - 1], chart_.offsetSeconds) ==
            EventTimeMs(route, updated[i], chart_.offsetSeconds))
            return false; // Exact timestamp collision; nothing committed.

    // Moving a dense group must remain a single reversible transaction.
    // Snapshot the complete touched cell range, including any pre-existing
    // neighbor events at the destination; unchanged events are not copied.
    SaveUndo(touchedMin, touchedMax);
    chart_.route.turns = std::move(updated);
    selectedTurnTimeMs_.reset(); // The selected source timestamp may have moved.
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
    SaveUndo(firstBeat, lastBeat);
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
    SaveUndo(firstBeat, lastBeat);
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

    SaveUndo(firstBeat, lastBeat);
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
    SaveUndo(firstBeat, lastBeat);
    events.erase(start, finish);
    MarkDirty();
    return true;
}

} // namespace locus::core
