# LOCUS — Former Wrap Seam Contract (retired)

**Status:** Superseded by the infinite-world route and stationary camera.

The former 4×4 modulo Wrap rule, clipped exit/re-entry line segments,
and on-screen teleport effects are **not active** in gameplay or the editor.
The core route now accumulates unbounded world coordinates, with eight
compass headings and one step per beat. The camera follows the fractional
world position by *translation only*, keeping the player fixed on the screen.

**PLAY and TEST:** no grid lines or grey dots; draw only the red route,
red turn targets and player (plus hit statistics in PLAY).

**ROUTE editor:** a grid remains visible, but is a scrolling editor viewport
whose width/height control density, never the movement boundaries.

Old v1 JSON files stay valid. Their previously wrapping route coordinates
intentionally produce a different, continuous path under this new mechanic.

See [CHART_FORMAT.md](CHART_FORMAT.md) for the current contract.
