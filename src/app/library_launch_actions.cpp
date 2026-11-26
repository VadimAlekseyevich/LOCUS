#include "app/library_launch_actions.h"

#include "app/editor_file_actions.h"
#include "app/game_session.h"
#include "app/library_playback.h"
#include "app/screen_flow.h"
#include "core/judgement.h"
#include "core/chart_revision.h"
#include "app/run_results.h"
#include <utility>

namespace locus::app {

bool LaunchLibrarySelection(const LibraryLaunchContext& ctx,
                            const MenuEvents& event, double now) {
    const LibraryLaunchIntent intent =
        ChooseLibraryLaunch(event, ctx.library, ctx.ui);
    switch (intent.kind) {
        case LibraryLaunchKind::None:
            return false;
        case LibraryLaunchKind::NewDifficulty:
            if (ctx.editorFiles.AnyDirty()) {
                ctx.ui.notice = "UNSAVED CHARTS RESTORED IN EDITOR; SAVE OR DISCARD FIRST";
                ctx.app.screen = Screen::Editor;
                return false;
            }
            if (intent.folder.empty()) {
                ctx.ui.notice = "SELECT AN INSTALLED SONG FIRST";
            } else if (ctx.editorFiles.NewDifficultyFromLibrary(
                    intent.audio.wstring(), intent.folder.wstring(), ctx.error)) {
                ctx.app.screen = Screen::Editor;
            } else {
                ctx.ui.notice = "NEW DIFFICULTY FAILED: " + ctx.error;
            }
            return false;
        case LibraryLaunchKind::EditDraft:
            ctx.app.screen = Screen::Editor;
            return false;
        case LibraryLaunchKind::EditChart:
            if (ctx.editorFiles.AnyDirty()) {
                ctx.ui.notice = "UNSAVED CHARTS RESTORED IN EDITOR; SAVE OR DISCARD FIRST";
                ctx.app.screen = Screen::Editor;
                return false;
            }
            if (ctx.editorFiles.OpenFromLibrary(intent.chart.wstring(),
                                                ctx.error))
                ctx.app.screen = Screen::Editor;
            else
                ctx.ui.notice = "EDIT FAILED: " + ctx.error;
            return false;
        case LibraryLaunchKind::PlayDemo:
            ctx.session.gameSession.SetRatePercent(ctx.ui.mods.RatePercent());
            ctx.session.customChart = false;
            ctx.session.activeChart = ctx.session.demoChart;
            ctx.session.chartTitle = "BUILT-IN DEMO";
            ctx.session.gameSession.ClearTestOrigin();
            ctx.session.judge.SetOffsetSeconds(0.0);
            ctx.app.screen = Screen::Play;
            return false;
        case LibraryLaunchKind::PlayChart: {
            // Always re-resolve after the library's file commands may rescan.
            int song = -1, chart = -1;
            if (!ctx.library.FindChart(intent.chart, song, chart)) {
                ctx.ui.notice = "PLAY FAILED: DIFFICULTY NO LONGER EXISTS";
                return false;
            }
            const LibraryChart& selected =
                ctx.library.Songs()[static_cast<std::size_t>(song)]
                    .charts[static_cast<std::size_t>(chart)];
            const LibraryPlaybackContext playback{
                ctx.session.song, ctx.loadedAudioPath,
                ctx.session.activeChart, ctx.session.chartTitle,
                ctx.session.chartPlayback, ctx.session.judge,
                ctx.session.gameSession, ctx.session.customChart
            };
            if (!ActivateLibraryDifficulty(selected, playback, now,
                                           ctx.error)) {
                ctx.ui.notice = "PLAY FAILED: " + ctx.error;
                return false;
            }
            ctx.session.gameSession.SetRatePercent(ctx.ui.mods.RatePercent());
            ctx.app.screen = Screen::Play;
            return true;
        }
    }
    return false;
}

bool LaunchSavedReplay(const LibraryLaunchContext& ctx,
                       const RunResultsStore& store, const RunResult& result,
                       ReplayLaunchInfo& info, double now) {
    core::ReplayTrace trace;
    if (result.judgementVersion != core::kJudgementRulesVersion) {
        ctx.error = "Replay uses older judgement rules.";
        return false;
    }
    if (!store.LoadReplay(result,trace,ctx.error)) return false;
    const bool previousCustom=ctx.session.customChart;
    ReplayLaunchInfo preparedInfo;
    if (result.songSetId=="builtin-demo" &&
        result.difficultyId=="00000000000000000000000000000001") {
        core::LoadedChart demo;
        demo.route=ctx.session.demoChart;
        if (trace.offsetSeconds!=0.0 ||
            !VerifyRunReplay(result,trace,demo.route,core::ChartGameplayRevision(demo))) {
            ctx.error="Replay does not reproduce the current demo.";
            return false;
        }
        ctx.session.customChart=false;
        ctx.session.activeChart=std::move(demo.route);
        ctx.session.chartTitle="BUILT-IN DEMO";
        ctx.session.judge.SetOffsetSeconds(0.0);
        ctx.ui.SelectSong(0,ctx.library);
    } else {
        int songIndex=-1, chartIndex=-1;
        const auto& songs=ctx.library.Songs();
        for (std::size_t i=0;i<songs.size() && songIndex<0;++i) {
            if (songs[i].id!=result.songSetId) continue;
            for (std::size_t j=0;j<songs[i].charts.size();++j) {
                if (songs[i].charts[j].metadata.difficultyId!=result.difficultyId) continue;
                songIndex=static_cast<int>(i);
                chartIndex=static_cast<int>(j);
                break;
            }
        }
        if (songIndex<0) {
            ctx.error="The recorded difficulty is not installed.";
            return false;
        }
        const auto& song=songs[static_cast<std::size_t>(songIndex)];
        const auto& chart=song.charts[static_cast<std::size_t>(chartIndex)];
        if (chart.gameplayRevision!=result.gameplayRevision) {
            ctx.error="The recorded chart revision is not installed.";
            return false;
        }
        const LibraryPlaybackContext playback{
            ctx.session.song,ctx.loadedAudioPath,ctx.session.activeChart,
            ctx.session.chartTitle,ctx.session.chartPlayback,ctx.session.judge,
            ctx.session.gameSession,ctx.session.customChart
        };
        const ReplayLaunchValidation validation{result,trace};
        if (!ActivateLibraryDifficulty(chart,playback,now,ctx.error,&validation))
            return false;
        preparedInfo.chart=chart.file;
        preparedInfo.background=song.backgroundImage;
        ctx.ui.SelectSong(songIndex+1,ctx.library);
        ctx.ui.SelectChart(chartIndex,ctx.library);
    }
    // Results may still be playing the previous song's outro. Stop the
    // other transport only after successful validation and preparation.
    if (previousCustom != ctx.session.customChart)
        ctx.session.gameSession.Stop(previousCustom);
    ctx.session.gameSession.SetReplay(std::move(trace));
    ctx.app.screen=Screen::Play;
    info=std::move(preparedInfo);
    ctx.error.clear();
    return true;
}

} // namespace locus::app
