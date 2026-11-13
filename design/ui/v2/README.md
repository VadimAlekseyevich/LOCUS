# LOCUS — screen layout wireframes

Open [index.html](index.html) in a browser. The frame inside the page is the **actual 960 × 540 virtual window** used by LOCUS. It scales down as a whole on narrow displays; its panels do not reflow into a website layout.

This is a **layout review**, not a finished UI theme. All in-frame controls are grayscale with square borders. There is no logo artwork, color palette, animation or decorative microcopy to approve at this stage.

## Navigation

Use the **Page** dropdown above the 960 × 540 frame to inspect any screen without changing the in-app navigation design. It is a *reviewer tool*, not part of the future application.

| Screen | Fixed layout |
| --- | --- |
| Home | Four large actions in one row and Exit |
| Song Library | Left: song tree and attempts of the selected chart; center: artwork, chart and large PLAY/EDIT; right: difficulty breakdown |
| Editor / SET | Song Set operations on the left; click-to-open difficulties on the right |
| Editor / ROUTE | Upper note ruler, editing tools, equal-sized comparison panes, 3×3 direction pad, editor-only speed/path; bottom whole-song overview; transport below |
| Editor / AUDIO | Difficulty name/preview offset; shared Song Set BPM markers |
| Editor / FIELD | Initial heading and a read-only field illustration |
| Settings | Game/Display/Audio settings on the left; key bindings always visible on the right |
| Skins | PATH/PLAYER/COLORS/MY SKINS controls on the left; one preview and profile actions on the right |
| Results | Details of a **specific chart attempt**; no separate Results section in application navigation |
| PLAY | **Static reference only**. No gameplay redesign or animation |
| Dialogs | Review-only selector for quit/unsaved/delete/etc. modals |

**Listen remains out of scope.** The current native C++ behavior is unaffected.

## Critical editor layout decision

The editor is **always the same five-zone vertical composition**:

    y =   0–56    chart name / SET · ROUTE · AUDIO · FIELD / Back
         56–118   upper zoomable NOTE RULER, snap & zoom
        118–438   workspace for current tab
        438–485   lower WHOLE-SONG OVERVIEW, shared tempo markers
        485–540   transport; ROUTE practice-rate buttons here

The editor uses no new full-height application sidebar, no overlapping toolbars and no lower-timeline relocation. The upper ruler contains notes; the bottom overview is independent of zoom and contains whole-song position and BPM markers.

The ROUTE workspace preserves **up to three equal-sized previews** and a separate note-direction pad. Clicking a pane changes focus, not its physical size. Starting heading belongs to FIELD; chart/song file operations belong to SET. Editing direction keys remain mouse buttons only.

## Results behavior

A future native implementation should finalize **one record per completed attempt at the final judged note**, store it under a stable chart identity with timestamps and judgments, open Results immediately and show multiple entries beneath the song tree when that chart is selected. Different charts must have separate history; a retry or abandoned attempt must not create a completed record.

The HTML mock has several in-memory sample attempts. **Simulate completion** appears in the review bar only on PLAY and demonstrates the flow. It is not a control proposed for scored gameplay. These records do **not** persist, and this work does **not** implement real scoring or disk storage.

## What to review first

- Are controls large enough and correctly grouped at **960 × 540**?
- Does every editor tab preserve both timeline positions?
- Can the Song Library show a song tree **and** multiple results without hiding PLAY and EDIT?
- Are SET/ROUTE/AUDIO/FIELD, Skins and Settings understandable without explanatory microtext?
- Do menus use only the space they need and keep primary actions visible?

Only after the geometry and workflows are approved should colors, type scales, borders, transitions and brand styling be designed again.

No C++ or chart/Song Set/skin/settings formats were changed.
