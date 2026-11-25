# LOCUS — UI design boundaries

Native HOME, song selection, SETTINGS, SKINS, RESULTS and editor controls use
RmlUi; raylib owns active gameplay and specialized canvases. The production
[UI architecture](../../../docs/UI_RML_MIGRATION.md) is authoritative.
The v1/v2 prototypes and tokens are visual references, not feature specifications.

## Design contract

- Keep app theme independent of gameplay SKIN. Reuse `assets/ui/rml/common.rcss`
  and semantic controls; fallback widgets use the shared `src/app/ui_*.h`
  foundation. Do not introduce another UI framework without a demonstrated need.
- Use adaptive native flex geometry and density-independent units. Shared measured
  rectangles drive canvas rendering and hit testing. The fallback virtual
  reference canvas is not a native layout constraint.
- Preserve visible keyboard focus, hover/selected/disabled states, readable
  feedback, UTF-8 text clipping and modal ownership. No actions on obscured controls.
- No persistent navigation header where Esc already supplies the action.
- Menus use the shared transition policy; gameplay timing, judgement,
  camera, route materials and HUD changes require separate gameplay design review.
  Do not use menu animations to alter scoring or authored timestamps.
- Preserve approved brand geometry; rebuild runtime PNGs from SVG sources
  when changing the approved artwork.

## Review

Use [Nightbloom review checklist](../v2/REVIEW.md), real Windows window sizes
and DPI, and the current [engineering acceptance](../../../docs/REFACTORING_PLAN.md).
Headless layout tests check geometry and input contracts; visual quality and
physical interaction require real hardware. Mockups do not establish implementation.
