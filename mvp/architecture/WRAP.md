# LOCUS — Former Wrap Seam Contract (retired)

**Status:** Superseded by the infinite-world route and stationary camera.

The former 4×4 modulo Wrap rule, clipped exit/re-entry line segments,
and on-screen teleport effects are **not active** in gameplay or the editor.
The core route now accumulates unbounded world coordinates, with eight
compass headings and one step per beat. The camera follows the fractional
world position by *translation only*, keeping the player fixed on the screen.

**PLAY and TEST:** no grid lines or grey dots; draw only the red route,
red turn targets and player in a centered window-height square (plus the **right-side**
accuracy and hit statistics when enabled). The red route is simply **clipped**
by the thin white-by-default, skinnable square border; the left side stays empty.

**ROUTE editor:** a grid remains visible, but is a scrolling square editor viewport.
One editable size (default 8) controls visibility density, never movement boundaries.

Old v1 JSON files stay valid. Their previously wrapping route coordinates
intentionally produce a different, continuous path under this new mechanic.

See [CHART_FORMAT.md](CHART_FORMAT.md) for the current contract.
