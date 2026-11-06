# LOCUS — Current Roadmap

**Updated:** 2026-11-06  
**Purpose:** Small, concrete product work, not the old first-playable checklist.  
**Product contract:** [GDD](GDD.md) · [UI / navigation](architecture/UI_FLOW.md) · [Chart v2](architecture/CHART_FORMAT.md)

The November 4–5 prototype roadmap and its acceptance notes are preserved in **Git history**. They describe a legitimately finished *historical* 4×4 Wrap prototype, **not the current game design or pending work**. Do not interpret old G-01…G-07 steps as requirements to recreate the old controls, renderer or JSON schema.

## Current playable baseline — implemented

- [x] **Core rhythm gameplay:** infinite continuous eight-way route, stationary marker, fading red future path, No Fail and Perfect/Good/Bad/Miss judgement.
- [x] **Centered square playfield:** square side follows physical window height in landscape, clips the visible trajectory, with customizable white-by-default border. No left-side gameplay labels.
- [x] **Current controls:** DFJK shared by game and editor for cardinal placement/diagonals; arrow-key timeline navigation and snap; seven remappable gameplay actions.
- [x] **Scoring:** 100/75/50/0 weighted accuracy, current % and grade counters on the right, final % and counts on Results; chart ends when the **last note is judged**, not at song outro.
- [x] **Local media:** WAV/MP3 listen mode, built-in demo, imported song library with multiple chart difficulties, recent-song filesystem persistence under user data.
- [x] **Editor:** ROUTE/AUDIO/FIELD tabs, always-visible timeline, field size 8 by default, hidden automatic start X/Y, BPM/whole-ms offset, independent snap subdivision and timeline zoom.
- [x] **Chart authoring:** place eight-way directions, 45°/90°/135° bends, group copy/cut/paste/invert/shift, undo/redo, save/open, audio preview and test-play from a chosen position.
- [x] **Chart v2:** stable absolute integer timeMs notes and offsetMs, local relative audioPath, backward reading of v1, song-duration checks and path validation.
- [x] **Visual skins:** independent global SKIN editor with configurable path/marker/HUD, ten RGBA colors (including FIELD BORDER), save/load and v1 skin compatibility.
- [x] **Navigation:** Home/PLAY/EDIT/SKIN/SETTINGS/EXIT, Esc-back behavior, unsaved-editor prompt, global Y/N quit confirmation for X/Alt+F4/Esc on Home.
- [x] **Windows development pipeline:** double-click BAT, reproducible pinned CMake dependencies, VS 2022 x64 Release, headless CTest, unit tests and diagnostics.

"Implemented" means code exists and has relevant automated checks; it does **not** certify every UI interaction or physical audio latency on the owner's machine.

## Next practical work (not completed)

These are the remaining **visible product and release debts**, not a mandate for an extensive architecture rewrite.

- [ ] **Hands-on regression run.** Play an imported chart, edit/reopen/test it, pause/retry/exit, test a last-note hit and last-note Miss, and verify skins. In particular, check the square/side HUD, clipping and confirmation dialogs on actual Windows displays. CI cannot see the real window.
- [ ] **Responsive UI on non-16:9 windows.** Verify narrow, ultrawide and portrait behavior for the central square and the right-side counters. Ensure no labels escape the visible region. Any resulting bug belongs to a focused UI fix, not a speculative layout overhaul.
- [ ] **Windows distribution.** Provide a straightforward playable build/ZIP or installer with tested clean-machine launch, without requiring the player to install CMake, Git or Visual Studio. Preserve the development BAT and optional audio probe.
- [ ] **Actual-song timing feel.** Gather feedback only from real play; if players report systematic latency, investigate calibration and measured device timing instead of changing the ±50/110/180-ms windows blindly.
- [ ] **Editor usability feedback.** Check discoverability of load/save/test, group-edit behaviour, shared DFJK bindings, fast BPM/offset edits and failures/recovery during file dialogs.
- [ ] **Release hygiene.** Before publishing a binary, decide explicitly on a source-code license, release version/notes, distribution dependencies, and support/troubleshooting instructions. The public GitHub repository alone does **not** grant a license.

No estimate or approval claim is attached to unchecked work. When it is done, update this checklist based on a real outcome.

## Deliberately deferred / not currently a requirement

Online chart servers and accounts, rankings, multiplayer, mobile ports, tempo maps, holds, automatic BPM detection, elaborate waveform editing, pitch-preserving slow motion and mandatory synthetic performance benchmarks. Consider them only if a future owner request makes them relevant.

## Working rules

1. Follow [GDD](GDD.md) and the **implemented code**, not obsolete archived first-prototype notes.
2. Prioritize one useful, playable change. Keep chart timing authoritative and the authored route independent of input.
3. Distinguish **CI passing** from **manual Windows UX/audio verification**.
4. Keep local audio files local, and preserve user charts/skin/settings whenever changes are made.
5. Prefer focused tests; do not reintroduce a 100-task backlog or a mandatory measurement gate.
