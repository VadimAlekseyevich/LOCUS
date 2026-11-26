# LOCUS

**Original game and source code by [VadimAlekseyevich](https://github.com/VadimAlekseyevich/LOCUS).**

A Windows rhythm game with a built-in chart editor. Read the moving route and press directional notes in time with your song, including repeated notes on straight sections. Missed notes change your grade, never the authored route.

UI reference: **[interactive Nightbloom prototype](design/ui/v2/index.html)**. HOME, song selection, Settings, SKINS and Results use RmlUi. The editor uses RmlUi for metadata, timing controls, difficulty selection and authoring controls, with raylib for timelines and route previews. Scored gameplay is rendered entirely by raylib. The rendering/input ownership rules are documented in [UI architecture](docs/UI_RML_MIGRATION.md); chart and gameplay logic are independent of the menu framework.

## Run

Double-click **`build_and_run.bat`** in the repository folder. It checks for Git, CMake 3.25+ and Visual Studio 2022 C++ Build Tools, offers to install missing tools, builds the game and launches it. It pulls Git updates with fast-forward only and does not overwrite local changes.

| Command | Action |
| --- | --- |
| `build_and_run.bat` | Build and run LOCUS |
| `build_and_run.bat build` | Build without launching |
| `build_and_run.bat test` | Build and run the game’s CTest suite |
| `build_and_run.bat package` | Build and create a portable Windows ZIP in the build directory’s `dist/` |

The Windows launcher chooses parallel build jobs from available logical CPUs and free physical RAM, up to 12 jobs (leaving two logical CPUs where possible and budgeting 1.25 GiB per job after a 2 GiB reserve). Set `LOCUS_BUILD_JOBS` to an integer from 1 to 64 to override this policy; for example, `set LOCUS_BUILD_JOBS=8` in Command Prompt before running the launcher. The selected value is printed and logged. The existing incremental build directory is reused.

A local log folder is created under `.locus-runs/` (Git-ignored). Logs are not uploaded. CI runs the same production CMake build and tests on Windows.

A portable Windows build is available as the **`locus-windows-x64`** [GitHub Actions artifact](https://github.com/VadimAlekseyevich/LOCUS/actions): download the contained `LOCUS-windows-x64.zip`, extract all files and run `Start LOCUS.bat` (or `locus.exe`). The launcher records startup output and the exit code in a temporary log and prints its path on failure. The complete `assets/ui` directory and `examples` ship beside the executable; CI diagnostics are a separate artifact. For the same portable layout locally, use `cmake --install build/windows --config Release --prefix build/windows/portable` after building. Required resources are declared in [`cmake/ui_assets_manifest.cmake`](cmake/ui_assets_manifest.cmake).

### Portable core development (Linux, Windows)

The rhythm/chart model can be compiled and tested without raylib, RmlUi, SoundTouch, a display or an audio device. Only the pinned JSON and Unicode dependencies are fetched.

```sh
cmake -S . -B build/core -DCMAKE_BUILD_TYPE=Release -DLOCUS_BUILD_APP=OFF -DLOCUS_BUILD_TESTS=ON -DLOCUS_BUILD_BENCHMARKS=ON
cmake --build build/core --config Release --parallel 2
ctest --test-dir build/core -C Release --output-on-failure
```

This configuration runs the 12 portable tests for route geometry, timing, judgement, chart JSON, editor history, difficulty and input/cursor models. It does not build the desktop game or validate native UI, disk transactions or audio hardware. The full Windows build remains the default. Linux CI also runs the editor model profiler.

## Gameplay

LOCUS is a four-key spatial rhythm game. Follow an authored route on an unbounded plane. Each note sets its absolute outgoing heading; a repeated heading requires another press without bending the route. The marker continues moving regardless of errors. Diagonals are two independently graded physical inputs at one timestamp.

| Action | Default binding |
| --- | --- |
| Left / Down / Up / Right | D / F / J / K |
| Diagonal | Both constituent directions (e.g. J+K) |
| Pause / retry / leave | Esc / R / H |
| Practice rate (custom chart) | 1–5: 100 / 75 / 50 / 25 / 10% |
| Movement speed | [ / ] or numpad − / + |
| Lookahead | , / . |

The largest centered square clips the forward route. Default speed is 135 logical px/s and lookahead 800 ms of original song time. Camera lag defaults to 180 ms; zero disables it. All are configurable in SKINS. The HUD shows combo and accuracy in the corners; detailed grades appear only when the right gutter permits. A Song Set background fills the window with aspect-preserving cropping.

Perfect/Good/Bad timing windows are ±50/110/180 ms. Accuracy is `(100*Perfect + 75*Good + 50*Bad) / resolvedInputs`. Miss breaks combo; there is no fail. The last resolved input triggers the outro and Results, even if music continues. Completed normal PLAY attempts are saved locally with maximum combo, timing statistics and scoring replay where available; TEST, spectator and abandoned attempts are excluded. WATCH REPLAY in native Results verifies and plays a matching installed revision without producing another score. See [GDD](docs/GDD.md), [timing](docs/TIMING_CONTRACT.md) and [results](docs/RESULTS.md).

## Song selection and mapping

HOME → PLAY opens a searchable song tree, with metadata and saved chart attempts on the left. Enter starts the selected difficulty when a text field is not focused. EDIT opens it in the editor; IMPORT and RESCAN manage local ingestion. Difficulty V4 ranks charts by physical speed, individual key strain, coordination, rhythm and stamina. Read* is diagnostic only. Ratings are computed on demand, never serialized.

HOME → EDIT opens a clean **Song Setup** draft. Attach WAV/MP3 and configure timing; select the draft and click it again or press Enter to open **Chart Editor**, installing the initial Song Set if necessary. Managed Song Sets expose title/artist/mapper/tags, shared tempo map, menu preview start, background and difficulty management in Song Setup. Esc returns to Song Setup without losing the draft.

In Chart Editor:

- The upper zoomable note ruler selects notes and patterns with snap; the lower whole-song overview seeks through audio and shows shared BPM markers. TIME copies whole milliseconds.
- Click the mouse-only 3×3 compass to place or change a note, including repeated headings; DEL removes it. Diagonal notes contribute to both constituent cardinal counts on the pad. START edits initial heading.
- Ctrl+click the ruler stages an unsnapped millisecond; the next direction click commits it. Distinct timestamps sharing one tick remain editable; repeated clicks cycle overlapping note clusters.
- Mirror horizontally/vertically, rotate ±90°, copy/cut/paste, shift and undo/redo patterns. COMPARE opens up to two additional same-song difficulties with independent draft/history and equal-sized synchronized previews. Focus changes neither seek nor rearrange panes.
- SPEED and PATH set editor-only visual speed (20–2000 logical px/s) and lookahead (50–10000 ms). Bottom Play/RST, whole-song overview and numeric 1–100% preview rate share one transport. SoundTouch preserves pitch on supported slow playback; fallback changes pitch. TEST plays the unsaved draft and returns to it.

Default shortcuts: O attach audio, Space preview, T TEST, Ctrl+S save focused difficulty, Ctrl+Shift+S new difficulty, Ctrl+O/Ctrl+N open/new, Ctrl+C/X/V copy/cut/paste, Ctrl+Z/Y undo/redo. Arrow keys navigate the upper ruler and adjust snap; Backspace/Delete removes notes. Save/Discard/Cancel protects dirty drafts; Save All leaves any failed draft open.

Song Setup provides NEW, DUPLICATE, IMPORT, EXPORT, DELETE CHART, DELETE SET, RESTORE, OPEN FOLDER and background controls. Export a complete set as `.locuspack` (store-only ZIP). Imports validate paths, CRC, bounds and shared audio identity; reimport merges persistent IDs without deleting unrelated difficulties. External chart saves install into the library rather than overwrite the source. Deletes are recoverable; chart overwrites retain backups.

## Settings and skins

SETTINGS has DISPLAY (VSync, FPS limit/counter, fullscreen), AUDIO / OFFSET (volume, offset/calibration) and PLAY/MENUS/EDITOR binding groups. Defaults: VSync on, 240 FPS cap, counter off, master volume 70%, offset 0. F11 toggles borderless fullscreen.

Global offset is an integer from −1000 to +1000 ms. Positive means audio is heard later than the raw clock; correction is subtracted from scored and visual song time, scaled by practice rate. CALIBRATE plays 100-BPM clicks: tap any bound gameplay key on each beat for 18 accepted taps, then APPLY, RETRY or CANCEL. This estimates audio delay plus tap bias, not hardware latency alone.

SKINS opens on the SKINS profile tab (rename, STORE/ACTIVATE, REFRESH, OPEN FOLDER, DELETE/UNDO), followed by PATH, PLAYER and COLORS. All tabs share one live gameplay preview. COLORS uses an HSV wheel, SV square and RGB/RGBA hex field. PATH controls preview duration, width, outline, fade, opacity, junction size/glow and reverse cap scale. PLAYER controls size, speed, camera lag, glow, flash and HUD. Eleven RGBA colors include FIELD BORDER and HIT ERROR BAR. LOAD imports a draft, SAVE applies it, SAVE AS exports it and RESET restores defaults.

Native menus omit persistent title/back bars: Esc navigates back, except during gameplay where it pauses. Leaving active play requires confirmation. Home Esc, EXIT, window close and Alt+F4 open quit confirmation. Gameplay direction and command bindings are configurable.

## Local data

Files live under `%LOCALAPPDATA%/LOCUS/`: `Songs/`, `Trash/`, `Results/`, `Skins/Profiles/`, `Skins/Trash/`, `Skins/active.locus-skin.json` and `settings.json`. Text uses UTF-8; Windows filesystem paths use Unicode APIs. Current pre-release contracts:

| Data | Version | Reference |
| --- | --- | --- |
| Charts | formatVersion 4 | [Format](docs/CHART_FORMAT.md), [example](examples/first-song.locus.json) |
| Song Set manifest | formatVersion 2 | [Storage](docs/SONG_SETS.md) |
| Skins | version 2 | [Example](examples/default.locus-skin.json) |
| Settings | version 3 | [GameSettings](src/app/game_settings.h) |
| Results | version 2; judgement rules 1 | [Results](docs/RESULTS.md) |

Backward compatibility before the first release is not guaranteed. Back up local data before format changes. The current portable ZIP is validated in CI, but clean-machine interactive acceptance and a tagged release remain pending.

## License and creator permissions

LOCUS's original game and source code are **source-available under the [PolyForm Noncommercial License 1.0.0](LICENSE)**. You may study, modify, build and share LOCUS for noncommercial purposes, but **you may not sell or commercially use the game, its code or modified builds without separate written permission**. This is *not* an OSI-approved open-source license because commercial software use is restricted.

**Original authorship must be preserved.** When distributing a copy or fork of LOCUS, include the license and the `Required Notice:` from [LICENSE](LICENSE), identifying **VadimAlekseyevich as the original LOCUS creator**. Do not present your fork as the original work of someone else.

There is a specific exception for community creators: **monetized videos, streams and original user-authored chart files are allowed**, including ads, sponsors, donations and paid chart downloads. These activities must credit **"LOCUS — original game by VadimAlekseyevich"** and link to this repository in a video/stream description, credits, or chart listing/README. See the [Community Content Permission](COMMUNITY_CONTENT_PERMISSION.md) for exact terms, including restrictions on selling or bundling the game itself.

Your chart and media rights remain yours; any music, artwork or other third-party material still requires permission from its rights holder. External dependencies retain their own licenses.

## Community chart packs with music — future hosting policy

**Design decision:** LOCUS is intended to let community members publish and download **complete playable chart packs, including the music**, in the manner familiar from osu!. A chart-only distribution requirement is **not** the plan. The present local Song Set/import/editor features are not an online hosting service; the rules below are the **policy to implement before a public upload/download service launches**, not a claim that takedowns or legal compliance are already operational.

### Separate rights and responsibilities

- **LOCUS software:** The game's source code and original assets are governed by [LICENSE](LICENSE), including the required credit to **VadimAlekseyevich** and the restriction on commercial software use. This license is **not** a license to any artist's music.
- **Community uploads:** Users will be the ones choosing, creating and submitting chart packs. Packs may contain authored chart data, audio recordings, music compositions, artwork and other files; each can have different rights holders. **The uploader must own the relevant rights or hold permissions/legal grounds for the actual storage, public distribution and use of everything uploaded**, including a full recording. Naming an artist or giving credit is not a substitute for permission. Uploading a commercial recording without permission can still infringe copyright even when LOCUS is free.
- **Chart authorship:** Mappers retain their rights in independently authored charts. The separate [Community Content Permission](COMMUNITY_CONTENT_PERMISSION.md) permits monetization of original charts and gameplay media **with LOCUS author credit**, but cannot authorize redistribution of someone else's music or artwork. Selling a chart does not automatically permit selling its bundled MP3.
- **Hosting permission:** The future online upload terms must explicitly grant the LOCUS service the limited permissions required to store, process, display and distribute submitted packs, without assigning ownership of users' original creations to LOCUS. These terms must not pretend to sublicense music rights the uploader does not have.
- **Featured / ranked status:** These are gameplay/community quality classifications, **not certifications of music licensing**. A ranked pack can still be reported and removed. The operator must not knowingly retain infringing material just because it is popular, ranked or manually reviewed.
- **No commercial exploitation of disputed uploads:** The intended model is not to sell, advertise against, or charge for access to unlicensed user-uploaded recordings. Any donations or other project funding do **not** make copyright infringement lawful or guarantee a liability exemption. Directly supplying infringing uploads as the operator is not made safe by calling them community content.

### Copyright reports and appeals — policy to activate before hosting

When the service launches, its published Terms of Service and copyright policy should establish the following real workflow:

1. Provide a **public, working contact/form** for copyright and other intellectual-property complaints. An actionable report should identify the copyrighted work(s), the precise pack/file/URL, enough information to locate it, the complainant's contact details, an authorized physical/electronic signature where required, and statements of good-faith belief and accuracy/authority as required by applicable law. Offer a route for incomplete reports to be supplemented.
2. Record the complaint and **promptly remove or disable public access** to material after a valid notice, actual knowledge or sufficiently clear indications of infringement, as applicable. A removal can cover the audio or the full downloadable pack, depending on the claimed rights and the law; do not silently restore complained-of audio from mirrors. Preserve only such records as are justified by law and operational need.
3. Inform the uploader of the action and reason when legally appropriate; permit a **genuine counter-notice/appeal for mistakes or authorized uses**, forward legally sufficient counter-notices as required, and follow the applicable reinstatement process. Under the US DMCA, a compliant counter-notice generally leads to reinstatement after **10–14 business days** unless the claimant reports an appropriate court action; do not replace this with an arbitrary automatic timer.
4. Adopt, announce and **reasonably enforce a repeat-infringer policy**, including suspension or termination of upload/service accounts in appropriate circumstances. Keep moderation decisions, notices and repeat-infringer records; detect and address materially identical reuploads without falsely promising universal automated copyright detection.
5. Provide understandable appeal and moderation procedures; prevent knowingly false claims or retaliation from becoming an unchecked removal tool, while still responding quickly to legitimate notices.

**Pre-launch legal/operational requirements:** Confirm who operates the hosting service, where it is established and whom it serves. If seeking the US DMCA section 512 hosting safe harbor, **appoint and publish a designated agent with usable contact details and register that agent with the US Copyright Office**; implement the notification, counter-notice and repeat-infringer processes and the other section 512 requirements, rather than just copying their wording. For services covered by EU rules, assess the [Digital Services Act](https://eur-lex.europa.eu/eli/reg/2022/2065/oj) notice-and-action, transparency, statements-of-reasons and applicable complaints obligations, plus any platform-specific music copyright licensing requirements. Obtain professional jurisdiction-specific review before accepting public packs.

**Status as of this documentation:** No designated DMCA agent or active copyright reporting inbox is represented by this repository text. No GitHub issue tracker, personal email address or placeholder is being advertised as a registered copyright agent. An actual service must publish its real contact and terms **before accepting uploads**. No notice, license disclaimer, lack of profit or user-upload checkbox by itself makes LOCUS immune to claims.

**Reference model:** [osu! copyright policy](https://osu.ppy.sh/legal/en/Copyright), [osu! user-upload terms](https://osu.ppy.sh/legal/en/Terms), [US Copyright Office section 512 guide](https://www.copyright.gov/512/) and [DMCA agent directory](https://www.copyright.gov/dmca-directory/). LOCUS intends to adopt comparable practical protections, **not** to claim that osu! policy automatically applies to LOCUS.

## Documentation

- [Game design](docs/GDD.md)
- [Roadmap](docs/ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Difficulty V4](docs/CHART_DIFFICULTY.md)
- [Input and audio timing](docs/TIMING_CONTRACT.md)
- [Native UI architecture](docs/UI_RML_MIGRATION.md)
- [Engineering acceptance checklist](docs/REFACTORING_PLAN.md)
- [Chart format](docs/CHART_FORMAT.md)
- [Song Set storage and recovery](docs/SONG_SETS.md)
- [Completed Results and scoring replay storage](docs/RESULTS.md)

The Windows CI checks compilation, chart/editor behavior and scoring without a GUI. Physical audio latency and desktop appearance still require real-device testing.

**Optional timing feedback:** SKIN → PLAYER → HIT ERROR BAR shows early/late successful physical arrow hits on a small bottom-center timing scale (original song-time windows). The newest marks fade after 1.25 seconds of real time, with at most 64 retained marks and no per-hit allocations. SKINS preview uses the same renderer; the bar is disabled by default, hidden with SHOW HUD, and older skin v2 files without `hud.hitErrorBar` default to off. PLAYER controls length (40–480 logical pixels); COLORS controls a single RGBA color. Only `hud.hitErrorLength` and `colors.hitErrorColor` are stored in addition to the visibility toggle.
