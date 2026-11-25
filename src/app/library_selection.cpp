#include "app/library_selection.h"
#include "core/unicode_search.h"

#include <algorithm>
#include <utility>

namespace locus::app {
namespace {
bool ContainsSearchKey(const std::string& normalized, const std::string& needle) {
    return normalized.find(needle) != std::string::npos;
}
void AppendSearchField(std::string& key, const std::string& field) {
    if (!key.empty()) key.push_back('\0');
    key += core::NormalizedSearchKey(field);
}
} // namespace

void LibrarySearchCache::Refresh(const SongLibrary& library) {
    if (source == &library && sourceRevision == library.Revision())
        return;
    // Build from the new authoritative snapshot before publishing its keys.
    // No gameplay/score data is copied; only normalized display/search fields.
    std::vector<std::string> nextMetadata;
    std::vector<std::vector<std::string>> nextTitles;
    nextMetadata.reserve(library.Songs().size());
    nextTitles.reserve(library.Songs().size());
    for (const auto& song : library.Songs()) {
        std::string key;
        AppendSearchField(key, song.title);
        AppendSearchField(key, song.artist);
        AppendSearchField(key, song.mapper);
        for (const auto& tag : song.tags) AppendSearchField(key, tag);
        nextMetadata.push_back(std::move(key));
        auto& titles = nextTitles.emplace_back();
        titles.reserve(song.charts.size());
        for (const auto& chart : song.charts)
            titles.push_back(core::NormalizedSearchKey(chart.metadata.title));
    }
    metadata.swap(nextMetadata);
    chartTitles.swap(nextTitles);
    source = &library;
    sourceRevision = library.Revision();
    queryValid = false;
}

const std::vector<LibrarySearchMatch>& LibrarySearchCache::Match(
        const SongLibrary& library,const std::string& text) {
    Refresh(library);
    if (queryValid && query == text) return matches;
    const auto needle = core::NormalizedSearchKey(text);
    // A case-only or canonically equivalent query has the same matches.
    if (queryValid && normalizedQuery == needle) {
        query = text;
        return matches;
    }
    std::vector<LibrarySearchMatch> next;
    static const std::string demoKey =
        core::NormalizedSearchKey("BUILT-IN DEMO ORIGINAL");
    if (ContainsSearchKey(demoKey,needle)) next.push_back({0,true});
    for (std::size_t i=0; i<metadata.size(); ++i) {
        const bool all = ContainsSearchKey(metadata[i],needle);
        if (all || std::any_of(chartTitles[i].begin(),chartTitles[i].end(),
                [&](const std::string& title) { return ContainsSearchKey(title,needle); }))
            next.push_back({static_cast<int>(i)+1,all});
    }
    matches.swap(next);
    query = text;
    normalizedQuery = needle;
    queryValid = true;
    ++matchGeneration;
    return matches;
}

const std::vector<LibraryTreeEntry>& LibraryTreeRows(const SongLibrary& songs,
                                                     const SongSelectUi& ui) {
    auto& cache = ui.treeCache;
    if (cache.source == &songs && cache.sourceRevision == songs.Revision() &&
        cache.expandedSong == ui.selectedSong && cache.query == ui.search)
        return cache.rows;
    std::vector<LibraryTreeEntry> entries;
    // Flatten only matched parents. Expanding another set no longer
    // performs a catalog-wide text search or renormalizes the query.
    const auto append = [&](int song,bool allCharts) {
        entries.push_back({song,-1});
        if (song != ui.selectedSong) return;
        const int difficulties = song == 0 ? 1
            : static_cast<int>(songs.Songs()[
                static_cast<std::size_t>(song-1)].charts.size());
        for (int chart=0; chart<difficulties; ++chart) {
            if (allCharts || ContainsSearchKey(
                    ui.searchCache.chartTitles[static_cast<std::size_t>(song-1)]
                                              [static_cast<std::size_t>(chart)],
                    ui.searchCache.normalizedQuery))
                entries.push_back({song,chart});
        }
    };
    if (ui.search.empty()) {
        const int count = static_cast<int>(songs.Songs().size())+1;
        entries.reserve(static_cast<std::size_t>(count)+20);
        for (int song=0; song<count; ++song) append(song,true);
    } else {
        const auto& matches = ui.searchCache.Match(songs,ui.search);
        entries.reserve(matches.size()+20);
        for (const auto& match : matches) append(match.song,match.allCharts);
    }
    cache.rows.swap(entries);
    cache.source = &songs;
    cache.sourceRevision = songs.Revision();
    cache.expandedSong = ui.selectedSong;
    cache.query = ui.search;
    ++cache.generation;
    return cache.rows;
}

std::size_t FindLibraryTreeRow(const std::vector<LibraryTreeEntry>& rows,
                               int song, int chart) noexcept {
    const auto found = std::lower_bound(rows.begin(),rows.end(),
        LibraryTreeEntry{song,chart},[](const auto& lhs,const auto& rhs) {
            return lhs.song < rhs.song ||
                (lhs.song == rhs.song && lhs.chart < rhs.chart);
        });
    return found != rows.end() && found->song == song && found->chart == chart
        ? static_cast<std::size_t>(found-rows.begin()) : rows.size();
}

namespace {
void KeepTreeSelectionVisible(SongSelectUi& ui, const SongLibrary& songs) {
    const auto& rows = LibraryTreeRows(songs, ui);
    const int maxScroll = std::max(
        0, static_cast<int>(rows.size()) - kLibraryTreeVisibleRows);
    ui.treeScroll = std::clamp(ui.treeScroll, 0, maxScroll);
    const auto index = FindLibraryTreeRow(rows,ui.selectedSong,
                                         ui.treeChartFocused ? ui.selectedChart : -1);
    if (index == rows.size()) return;
    const int i = static_cast<int>(index);
    if (i < ui.treeScroll) ui.treeScroll = i;
    if (i >= ui.treeScroll + kLibraryTreeVisibleRows)
        ui.treeScroll = i - kLibraryTreeVisibleRows + 1;
}
} // namespace

void SongSelectUi::ReplaceResults(RunKey key,
                                  std::vector<RunResult> loaded,
                                  std::string gameplayRevision) {
    resultKey = std::move(key);
    resultGameplayRevision = std::move(gameplayRevision);
    results = std::move(loaded);
    resultScroll = 0;
    ++resultsRevision;
}

void SongSelectUi::SelectSong(int index, const SongLibrary& songs) {
    selectedSong = std::clamp(index, 0,
                             static_cast<int>(songs.Songs().size()));
    selectedChart = 0;
    firstVisibleChart = 0;
    treeChartFocused = false;
    // A difficulty-title search must not silently select a hidden sibling.
    if (!search.empty()) {
        const auto& rows = LibraryTreeRows(songs,*this);
        const auto parent = FindLibraryTreeRow(rows,selectedSong,-1);
        if (parent < rows.size() && parent+1 < rows.size() &&
            rows[parent+1].song == selectedSong)
            selectedChart = rows[parent+1].chart;
    }
    KeepTreeSelectionVisible(*this, songs);
}
void SongSelectUi::MoveSong(int delta, const SongLibrary& songs) {
    SelectSong(selectedSong + delta, songs);
}
void SongSelectUi::SelectChart(int index, const SongLibrary& songs) {
    const auto* song = selectedSong > 0 &&
                       selectedSong <= static_cast<int>(songs.Songs().size())
        ? &songs.Songs()[static_cast<std::size_t>(selectedSong - 1)] : nullptr;
    const int count = song ? static_cast<int>(song->charts.size()) : 1;
    selectedChart = std::clamp(index, 0, std::max(0, count - 1));
    treeChartFocused = true;
    KeepTreeSelectionVisible(*this, songs);
}
void SongSelectUi::MoveChart(int delta, const SongLibrary& songs) {
    if (!delta) return;
    if (search.empty()) {
        SelectChart(selectedChart + delta,songs);
        return;
    }
    const auto& rows = LibraryTreeRows(songs,*this);
    const auto parent = FindLibraryTreeRow(rows,selectedSong,-1);
    if (parent == rows.size() || parent+1 >= rows.size() ||
        rows[parent+1].song != selectedSong) return;
    const auto first = parent+1;
    const auto end = std::upper_bound(rows.begin()+first,rows.end(),selectedSong,
        [](int song,const LibraryTreeEntry& entry) { return song < entry.song; });
    const auto last = static_cast<int>(end-rows.begin()-1);
    const auto current = FindLibraryTreeRow(rows,selectedSong,selectedChart);
    const int next = current == rows.size()
        ? (delta > 0 ? static_cast<int>(first) : last)
        : std::clamp(static_cast<int>(current)+delta,static_cast<int>(first),last);
    SelectChart(rows[static_cast<std::size_t>(next)].chart,songs);
}
void SongSelectUi::MoveTree(int delta, const SongLibrary& songs) {
    const auto& rows = LibraryTreeRows(songs, *this);
    if (rows.empty() || !delta) return;
    const auto position = FindLibraryTreeRow(rows,selectedSong,
        treeChartFocused ? selectedChart : -1);
    const int next = position == rows.size()
        ? (delta > 0 ? 0 : static_cast<int>(rows.size())-1)
        : std::clamp(static_cast<int>(position)+delta,0,static_cast<int>(rows.size())-1);
    const auto target = rows[static_cast<std::size_t>(next)];
    if (target.chart < 0) SelectSong(target.song, songs);
    else {
        if (selectedSong != target.song) SelectSong(target.song,songs);
        SelectChart(target.chart,songs);
    }
}
void SongSelectUi::ScrollTree(int delta, const SongLibrary& songs) {
    const auto& rows = LibraryTreeRows(songs, *this);
    treeScroll = std::clamp(treeScroll + delta, 0,
        std::max(0, static_cast<int>(rows.size()) - kLibraryTreeVisibleRows));
}
const LibraryChart* SongSelectUi::Selected(const SongLibrary& songs) const {
    if (selectedSong <= 0 ||
        selectedSong > static_cast<int>(songs.Songs().size())) return nullptr;
    const auto& charts = songs.Songs()[static_cast<std::size_t>(selectedSong - 1)].charts;
    if (selectedChart < 0 ||
        selectedChart >= static_cast<int>(charts.size())) return nullptr;
    return &charts[static_cast<std::size_t>(selectedChart)];
}

} // namespace locus::app
