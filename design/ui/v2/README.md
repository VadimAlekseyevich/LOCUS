# LOCUS — monochrome layout wireframes

Open [index.html](index.html) in a browser. The inner frame is the native LOCUS **960 × 540** virtual viewport. The external **Page** picker is for reviewing screens and is **not** a proposed game control.

There is no product theme, finished artwork, animation or native integration yet. This change affects **only HTML wireframes**. C++ gameplay and project file formats are unchanged.

## PLAY means song and chart selection

The **PLAY button on Home opens the song selector**, which is internally called `library` in the prototype. This is **not the scored gameplay screen**.

The song selector uses LOCUS's existing concepts, rearranged for easy browsing:

- **Right:** song tree with expandable difficulty rows, chart ratings and search.
- **Center:** large background/artwork placeholder, selected song title, artist and mapper, and large PLAY / EDIT actions.
- **Left:** selected difficulty, note count, difficulty breakdown and **personal results for the selected chart** (not an invented global ranking).

The separate **Gameplay — static reference** reviewer option exists only to exercise the Results demonstration. Its actual on-screen composition is not being redesigned. Native gameplay uses a centered 540×540 field, right-side statistics and an empty left margin, with no added UI animations.

## Editor layout at 960×540

All three editor tabs share these fixed zones:

| Vertical pixels | Function |
| --- | --- |
| 0–56 | Back, chart title, SET / ROUTE / AUDIO |
| 56–121 | Upper **note ruler**, with separate Snap − / + and Zoom − / + |
| 121–485 | Editing workspace |
| 485–540 | **Whole-song timeline** with small Play / Restart icons on the left; playback-rate dropdown and copyable millisecond timestamp on the right |

There is **no separate bottom transport row**, and the lower overview remains independent of the upper note ruler's zoom. The two timelines never swap places.

### SET

Editable **Song title, Artist, Mapper, Tags**. Buttons have intentional pairings by row: **Background / Remove BG**, **Import / Export**, then **Open folder / Restore**. Difficulties are on the right, clicked directly to open; New / Duplicate and deletions remain accessible.

In this browser demo, changing metadata and pressing **Ctrl+S** updates the example Song Set in memory only. Tags are entered as comma-separated text. No project files are written.

### ROUTE

Removed the visible Copy / Cut / Paste / Undo / Redo toolbar; those operations keep their **keyboard shortcuts**. The main field is larger. Small vertical icon buttons on its **left** control horizontal/vertical reflection, ±90° rotation, chart comparison and Test. Hovering/focusing exposes a short description and the shortcut **where one exists**; rotation and comparison have no fabricated keyboard shortcuts.

At right are the 3×3 note placement pad, **initial heading** (previously the entire FIELD tab), editor speed and path preview length. Initial heading is no longer a separate page.

One, two, or three comparison panes keep **equal sizes** for the active number of panes. The note ruler and song overview retain their positions.

### AUDIO

Same chart title, preview start and shared BPM/offset marker table.

## SETTINGS

Only **DISPLAY** and **AUDIO** pages remain. VSync, FPS limit, FPS counter and fullscreen belong to Display. Master volume, global offset and calibration belong to Audio. Game and Hit flash toggles have been removed from these Settings wireframes; hit flash **size** remains a Skin/Player control. The separate PLAY / MENUS / EDITOR key-binding groups remain on the right.

## SKINS and Results

Skins keeps PATH, PLAYER, COLORS and MY SKINS, with a persistent preview. Results stay attached to a **specific chart attempt**, visible from the chart's history on the PLAY song selector; several attempts may exist per chart.

A future native implementation should record **one completed attempt at the final judged note** with stable chart identity, timestamp, judgment counts and accuracy, then open that attempt's Results. Abandoned/retried sessions must not count as completed attempts. The external **Simulate completion** review control appends an example *in-memory* attempt; it does **not** exist in native scored gameplay.

## Review priorities

Review **placement and control sizes first**, especially the new PLAY song selection, SET metadata rows, expanded ROUTE preview and single lower timeline row. Everything remains grayscale until the layout is approved.

No native UI, gameplay code, Song Set/chart/skin format or persistent Results storage was modified.
