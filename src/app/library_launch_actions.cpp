#include "app/library_launch_actions.h"

#include "app/editor_file_actions.h"
#include "app/game_session.h"
#include "app/library_playback.h"
#include "app/screen_flow.h"
#include "core/judgement.h"

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
            ctx.app.screen = Screen::Play;
            return true;
        }
    }
    return false;
}

} // namespace locus::app
