# LOCUS

**Original game and source code by [VadimAlekseyevich](https://github.com/VadimAlekseyevich/LOCUS).**

A Windows rhythm game with a built-in chart editor. Read the moving route and press the outgoing direction in time with your song; missed notes change your grade, never the authored route.

## Run

Double-click **`build_and_run.bat`** in the repository folder. It checks for Git, CMake 3.25+ and Visual Studio 2022 C++ Build Tools, offers to install missing tools, builds the game and launches it. It pulls Git updates with fast-forward only and does not overwrite local changes.

| Command | Action |
| --- | --- |
| `build_and_run.bat` | Build and run LOCUS |
| `build_and_run.bat build` | Build without launching |
| `build_and_run.bat test` | Build and run the game’s CTest suite |

A local log folder is created under `.locus-runs/` (Git-ignored). Logs are not uploaded. CI runs the same production CMake build and tests on Windows.

The audio offset is stored as an optional `globalOffsetMs` integer in the current `settings.json` v3; existing key bindings and saved charts are preserved. No chart, skin or Song Set file format changes.

## Gameplay

The world is infinite. During PLAY, an experimental camera **slightly lags behind the moving player** instead of anticipating the route: the player appears ahead of center as the camera catches up. The lag scales with movement speed (up to a safe limit), and turns are smoothed without reading future notes. The **red forward route moves with the camera**. Only the part inside the centered square is visible. In landscape the square's side equals the **window height**; a thin white frame can be recolored in SKIN. No gameplay grid or text is drawn on the left.

To the right, the game shows **Accuracy** and counts for Perfect, Good, Bad and Miss. Accuracy is `(100×Perfect + 75×Good + 50×Bad) / all judged notes`; a Miss contributes zero. A chart ends **when its final note is judged**, not when the music file ends. Results shows the final accuracy and counts.

| Game action | Default key |
| --- | --- |
| Left / Down / Up / Right | **D / F / J / K** |
| Diagonal | Two adjacent directions together (for example J+K) |
| Pause / retry / Home | **Esc / R / H** |

Gameplay direction bindings are configurable in SETTINGS. **SETTINGS → GAME → GLOBAL AUDIO OFFSET** accepts a signed integer in milliseconds (−1000 to +1000), or use **CALIBRATE** to start a synthesized 120-BPM metronome: listen for the high accent every four beats and tap **Space** on each accent. Two accents are warm-up; eight valid taps produce a median-based suggested delay, with noisy attempts rejected. Choose **APPLY** to save the suggestion, **RETRY** to measure again, or **CANCEL** to keep your current value. **Positive offset (+200 ms)** means the output is heard 200 ms later than the raw song clock; **negative offset (−200 ms)** means it is heard earlier. The global correction moves both scored input/judgement and gameplay visuals to the same perceived audio time in PLAY and TEST, including the built-in demo. It does **not** change chart `timeMs`, song offset, tempo markers, editor clock, or audio playback speed. Calibration measures audio-output delay **plus the player's tap tendency**; repeat a few times for consistency and preferably use wired headphones.

Editor note placement uses only the clickable direction pad. During PLAY (including editor TEST), a fresh press of any bound direction during a long unmapped intro skips the music forward to the first note minus the smallest whole number of **4/4 measures lasting at least one second**. The triggering press is not scored; near-note presses and normal music with no long intro are unaffected.

## Screens and editor

Home has **PLAY, EDIT, SKIN, SETTINGS, EXIT**. PLAY opens a read-only library with a song tree on the right: selecting a song expands its indented charts below it, sorted by calculated difficulty (easiest first). Up/Down navigate rows, Left/Right switch difficulties; use **EDIT** for content changes. Each chart displays an uncapped **DIFFICULTY V2** rating, with separate speed, rhythm, key coordination, reading and stamina estimates. The rating is also visible live in EDIT → ROUTE / AUDIO. See [Chart Difficulty](docs/CHART_DIFFICULTY.md) for the complete heuristic and its limitations. Installed songs live under `%LOCALAPPDATA%\LOCUS\Songs`. The built-in demo is always available.

EDIT has four tabs and two independent timelines. The **upper note ruler** scrolls with zoom/snap and shows authored turns. The **bottom whole-song overview** always spans the exact audio duration, never zooms and shows the Song Set's shared BPM markers rather than chart notes. Click or drag the bottom bar to jump to distant sections at the closest snap tick without modifying the chart. All compared previews follow the same selected song time:

- **SET:** Song Set, shared audio and difficulties (easiest first). **Click a difficulty to open it directly**; unsaved draft changes require confirmation. Use NEW, COPY, IMPORT, EXPORT, DELETE/RESTORE and OPEN FOLDER. The list scrolls with the mouse wheel, without paging buttons.
- **ROUTE:** larger single-purpose editing buttons, TEST, an enlarged 222×222 route preview and full-size 3×3 mouse-only direction pad. **COMPARE** opens at most two other difficulties of the same Song Set, for three equally sized, exact-time synchronized previews. The **SPD −/+** and **PATH −/+** controls beside the pad set editor-only movement speed (px/s) and visible path length (ms), saved separately from the gameplay SKIN. Click a preview to focus that chart **without moving or resizing its pane or seeking the song**: the upper ruler and lower song overview remain at exactly the same music time, even with different tick resolutions or paused playback. Its notes and tools appear above, while the single audio playback stays shared. Each open chart retains its own Undo/Redo and unsaved route; nonfocused unsaved panes show an asterisk. Practice-speed buttons stay below the whole-song overview. Copy/paste, shift, mirror, rotations and undo/redo are available.
- **AUDIO:** difficulty name, an editable **song-wide BPM table** (absolute offset in whole milliseconds and BPM per row), plus the independent Song Preview Start. Add a marker at the selected song position, edit its time/tempo, or delete a later marker. The first row establishes beat zero; later rows change BPM without restarting beat count. The table and markers apply to every difficulty in the pack.

Chart titles, song/artist names, difficulty labels and saved skin names are stored in **UTF-8**. Input, paste, Backspace and shortened UI labels preserve whole Unicode characters (Cyrillic, accents, etc.). Glyph rendering depends on the installed font; the Windows UI font atlas covers Cyrillic, Latin with accents and Greek.
- **FIELD:** initial compass heading, selectable by its left/right buttons. Current SKIN base speed and preview time are shown read-only; no independent editor grid-size control.

Click the eight compass directions or the center DEL button to place/remove notes; gameplay direction keys do not place editor notes. On the upper note ruler, **←/→** navigate and **↑/↓** adjust snap subdivision; PageUp/PageDown, wheel and modifier combinations also work. Load audio with **O**, preview with **Space**, test-play with **T**; in PLAY, Esc opens pause/menu. Practice speeds in ROUTE are 100/75/50/25/10%. **Ctrl+S** saves ONLY the currently focused difficulty into the permanent Song Set library (renaming a difficulty also renames its JSON file, with a safe suffix if needed), **Ctrl+Shift+S** creates a new difficulty in that set, **Ctrl+O** opens a chart, and **Ctrl+N** starts a new draft. External chart files are not silently overwritten: saving their edited versions installs them into the library.

To share a complete song with all difficulties, use **EDIT → SET → EXPORT** to export a `.locuspack` archive. **EDIT → SET → IMPORT** accepts individual `.locus.json` charts or complete `.locuspack` packages. The archive is a standard ZIP container with stored files, song metadata, shared audio and all difficulties. Imports check paths, checksums and file size limits before installing anything.

Open a song with **EDIT**, then use **SET → NEW / COPY / DELETE / DELETE SET / RESTORE LAST** to manage difficulties and songs. Deletions require **Y** confirmation and can be recovered from LOCUS Trash. **AUDIO → SONG PREVIEW START (MS)** accepts a manually typed whole-millisecond value (Enter saves, Esc cancels, Ctrl+V pastes); the +/− buttons are removed. Copied ROUTE note patterns remain available when switching difficulties or opening a new draft; Ctrl+V pastes using the destination chart's current tick resolution (incompatible tick grids are rejected without modifying the chart). On **ROUTE**, click the **TIME** value above the direction pad to copy its current song position as an integer number of milliseconds; the upper note ruler itself has no time labels. The lower overview shows the song end and all shared BPM-change markers. **ADD/REPLACE BG** and **REMOVE BG** manage shared song artwork. On **ROUTE**, the **ROT -90** and **ROT +90** buttons rotate the selected pattern’s absolute headings counterclockwise/clockwise without shifting timestamps. Select a note and press Backspace/Delete (or the middle **DEL** pad button) to remove it; Ctrl+Z undoes edits. All difficulties in a managed Song Set share exactly one audio file. See [Song Set storage](docs/SONG_SETS.md).

Chart JSON uses **formatVersion 4** only, absolute heading codes `u/r/d/l/ru/rd/ld/lu`, integer `timeMs` and independent snap settings. Older chart versions and full-length headings are not supported. BPM changes do not move already authored notes in song time. See [chart format](docs/CHART_FORMAT.md) and [sample](examples/first-song.locus.json) (requires your own `song.mp3`).

SKIN now has **PATH, PLAYER, COLORS and MY SKINS** tabs. COLORS uses a hue wheel, a saturation/brightness square and an exact `#RRGGBB` / `#RRGGBBAA` field instead of four RGBA sliders; the live gameplay preview stays visible while changing colors. MY SKINS scans `%LOCALAPPDATA%\\LOCUS\\Skins\\Profiles`; **OPEN FOLDER** opens that directory in Explorer; choose **ACTIVATE** to apply a saved profile, **STORE** to create another named profile, **REFRESH** to rescan, **DELETE** to remove the selected profile with confirmation, or **UNDO** to restore the most recent deletion from `Skins\\Trash`. The active look remains in `Skins\\active.locus-skin.json`. **LOAD** imports an external JSON into the draft, **SAVE** applies changes, and **SAVE AS** exports a JSON. Switching profiles refuses to discard unsaved edits. SKIN → PLAYER also provides **CAMERA LAG (MS)** (180 by default; 0 disables the effect; slider 0–500, exact entry up to 1500), saved separately in every skin. SKIN also edits path effects, marker size, **fixed cardinal movement speed** (20–2000 logical pixels/s; default 135), HUD visibility and ten RGBA colors including the square border. Speed is independent of BPM, chart grid size, practice playback rate and heading: diagonal movement is normalized to the same distance per tick as straight movement. The ROUTE editor shows continuous exact-time preview routes without grid lines or dots. All ROUTE panes use separate editor-only movement speed and path-length settings; these do not change the gameplay SKIN. Grid metadata does not scale the preview. The snap ruler remains editable. During PLAY, **[ / ]** or **numpad − / +** adjust the speed by 10 pixels/s and save it for future charts. The [skin sample](examples/default.locus-skin.json) is independent of charts.

**Esc means Back** in Editor and menus, **Pause/Menu** in PLAY. After **EDITOR → TEST**, leaving the level returns directly to the **same unsaved editor draft**; TEST never auto-saves. Normal PLAY requires confirmation before leaving for Song Library. Windows F11 uses a non-topmost borderless desktop window so Alt+Tab can switch applications. Closing an unsaved comparison chart or leaving Editor prompts **S/Save**, **Y/Discard**, or **N/Esc/Cancel**. Saving on exit writes all open changed charts; a failed write does not close any remaining unsaved draft. New unsaved difficulties must be saved once before they can be parked in a comparison pane. **Esc on Home**, EXIT, the window X and Alt+F4 open a quit confirmation; **Y** quits and **N/Esc** cancels.

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

## Project

- [Game design](docs/GDD.md)
- [Roadmap](docs/ROADMAP.md)
- [Architecture audit](docs/ARCHITECTURE.md)
- [Chart format](docs/CHART_FORMAT.md)
- [Song Set storage and recovery](docs/SONG_SETS.md)

The Windows CI checks compilation, chart/editor behavior and scoring without a GUI. Physical audio latency and desktop appearance still require real-device testing.
