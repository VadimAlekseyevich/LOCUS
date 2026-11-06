# LOCUS — Current UI Flow and Controls

**Status:** Implemented interaction guide, 2026-11-06.  
The original proposed 4×4 wireframes are retained in Git history, not as an active design requirement. See [GDD](../GDD.md) for the current gameplay concept.

## 1. Screens

\`\`\`text
HOME
├── PLAY ── Song library + difficulty selector ── Scored PLAY ── RESULTS
│          ├── Built-in demo               ↑        │          ├── Retry
│          ├── Imported WAV/MP3 + chart    └────────┘          └── Home
│          ├── Import chart
│          └── Edit installed difficulty ────────────────┐
├── EDIT ── Chart editor ◄───────────────────────────────┘
│          ├── ROUTE (path preview + nine-way direction pad)
│          ├── AUDIO (title, BPM, offsetMs)
│          └── FIELD (one NxN size + initial heading)
│          ├── Preview local song
│          └── TEST ── Scored PLAY ── RESULTS
├── SKIN ── Global visual profile editor
├── SETTINGS ── Volume, hit flash, fullscreen, chord timing, key binds
└── EXIT ── Confirmation: Y quit / N stay
\`\`\`

The separate **LISTEN** view is for playback of a local WAV/MP3 without chart judgements. The built-in demo is always available without selecting a user audio file.

## 2. Home, library and editor

- Home has **PLAY, EDIT, SKIN, SETTINGS, EXIT**. PLAY opens the installed-song browser; songs and their chart difficulties are selected separately.
- Library keyboard navigation: **Up/Down** songs, **Left/Right** difficulties, **Enter** to play, **C** to import a chart, **E** to edit the selected difficulty, **F5** to rescan. It also has clickable actions and BACK.
- The song library imports chart+audio into per-song folders under the user's LOCUS data directory. Do not describe this as online hosting.
- Editor has **ROUTE, AUDIO, FIELD** tabs, persistent playback controls and one horizontally scrolling timeline. A fixed playhead remains visible while the time ruler scrolls.
- ROUTE: scrolling preview lattice, nine outgoing directions including diagonal chords and DEL, heading control, note operations and speed buttons.
- AUDIO: direct text values for song title, BPM and integer millisecond offset; typing does not trigger gameplay hotkeys.
- FIELD: **one number** (2–16, default 8) determines both square sides; heading can be rotated. **No start X/Y input**.
- Notes use configurable **gameplay direction keys**, default D/F/J/K, not hard-coded editor-only DFJK shortcuts. An adjacent two-key stroke places a diagonal.
- Timeline uses **Left/Right** arrow navigation and **Up/Down** subdivisions by default; PageUp/PageDown and wheel also navigate. Ctrl/Shift modifiers provide bar jumps, note movement and selected group movement. Editor shortcuts yield to rebound gameplay directions when colliding.
- Editor controls include New/Open/Save/Save As/Test and Undo/Redo; unsaved changes are protected by a confirmation when leaving or replacing the chart. Ctrl+S, Ctrl+Shift+S, Ctrl+O, Ctrl+N and T are available.
- TEST starts from the selected or paused audio position and scores only notes from that point onward; skipped earlier notes are not Misses.

## 3. Scored Play and Results

\`\`\`text
+---------+----------------------------+------------------+
|         | thin white/custom frame    | ACCURACY         |
|  empty  |                            |  87.50%          |
|         |   square, centered,        | PERFECT          |
|  left   |   moving red future route  | GOOD             |
|  area   |   + stationary marker      | BAD              |
|         |                            | MISS             |
|         |                            |                  |
+---------+----------------------------+------------------+
\`\`\`

The diagram is schematic. In landscape, the square's side matches the *actual window height*; a 960×540 reference contains a centered 540×540 square. The world does **not** wrap when the trajectory reaches the white border. The path is simply clipped. The left side must stay **completely empty**, with no clipped wordmark or debug labels. On non-16:9 windows, the right HUD must fit the actually visible sidebar width. On portrait windows, the square is width-limited.

The right HUD contains **current accuracy** and **Perfect/Good/Bad/Miss** counts. If the skin hides the HUD, the score panel disappears; the center square remains. The global skin can change the field border color.

When the **last scorable note** is graded, music stops and RESULTS opens immediately, rather than waiting for an audio outro. The result view shows the **same four counts, final accuracy and resolved-note count**. Results can Retry or return Home.

- Default gameplay cardinal keys: **D=Left, F=Down, J=Up, K=Right**. Two adjacent directions form a diagonal within the chord window.
- **Space** pauses, **R** restarts, **H** returns Home by default. All are rebindable.
- No Fail: the chart route is unaffected by grades or missed inputs.

## 4. Escape, close, and unsaved edits

| Action | Outcome |
| --- | --- |
| **Esc** in Editor (not editing text) | Back to Home; dirty chart prompts **Y discard / N or Esc stay** |
| **Esc** in an editor text field | Cancel that field; do not leave the editor |
| **H** or visible **BACK** in Editor | Return Home, with the same dirty-chart check |
| **Esc** in Play/Results/Library/Listen/Skin/Settings | Back to Home |
| **Esc** on Home or **EXIT** button | Open **EXIT LOCUS?** |
| Window **X** or **Alt+F4** on any screen | Open the same exit confirmation, without closing immediately |
| **Y** / QUIT in confirmation | Close the program |
| **N**, **Esc** / STAY in confirmation | Cancel and resume the prior screen/playback |

The exit dialog warns about unsaved chart or skin drafts. The current Windows close guard makes it possible to cancel X/Alt+F4 without a permanently latched close event.

## 5. Areas intentionally not specified by this document

No old Wrap seam, 4×4 point-grid, two-button relative-key scheme, editable start coordinates, mandatory waveform lane, or cloud chart browser belongs to the current UI. Do not implement a feature solely because an old proposed wireframe mentions it.

Windows CI covers input/state logic and layouts headlessly. The actual appearance and physical device behavior remain subject to hands-on Windows testing.
