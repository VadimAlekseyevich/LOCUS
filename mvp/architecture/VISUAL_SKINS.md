# LOCUS — Visual customization boundary

The current game uses one built-in look. This document records the minimal
architecture seam needed for **future player-controlled skins**, not a promise
that skins are already configurable.

## Implemented now

- `src/app/visual_style.h` owns the default **route visual palette**:
  high-contrast preview color, outline, current/traveled path colors,
  and preview stroke widths. Both editor and scored gameplay use these values.
- The same header contains an **editor layout token** for the vertical
  practice-speed control group. Hit testing and rendering share its bounds.
- The route sampler, chart JSON, timing clock and scoring contain **no
  colors, textures or screen coordinates**. A visual update cannot silently
  change when/where a turn happens.
- Layout changes are still developer-controlled. There is **no settings UI,
  skin file, texture loading, persistent style selection or drag-and-drop**.

## Direction for a later skin system

1. Split authoring/gameplay layouts into named anchors and size constraints
   (grid center, HUD, timeline, score text, transport) under a common
   `VisualStyle` model. Keep hitboxes generated from the same anchors used
   to draw controls, including at different resolutions.
2. Allow a user skin to override specific palette values, optional textures,
   typography and anchors, falling back to these known-good defaults.
3. Validate skin data (reasonable contrast, finite positions, bounded sizes,
   safe texture paths/resources). A malformed skin must never break gameplay
   or make controls inaccessible; offer reset to defaults.
4. Persist presentation preferences **separately from chart files**. A shared
   chart must play identically regardless of the viewer's skin. Do not store
   arbitrary player-specific layout in `.locus.json`.
5. Preview edits live, then make Save/Cancel/Reset explicit; add keyboard
   accessibility and resolution scaling before exposing freeform dragging.

For now, preserve the small default style contract and focus on reliable
authoring and playback.
