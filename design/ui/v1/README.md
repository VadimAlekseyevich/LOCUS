> **New:** The current full-application interactive UI design is [Interface Studio v2](../v2/index.html). This v1 folder remains the source of approved vector branding and the original Home concept.

# LOCUS — approved UI reference (v1)

This folder is the **approved design reference**, not C++ gameplay code. Theme, widget, text-fit, focus and motion foundations exist in `src/app/ui_*.h`. **Home is integrated in C++**; Library, Settings, SKIN, Editor and other screens are not yet migrated.

## Locked direction

- Centered main menu composition based on the approved concept **02**.
- Main wordmark: soft lowercase **locus.**, with rose dot. Original vector outlines from concept 02 retained, with SVG viewBox cropped to the artwork.
- Compact mark: **LO** from concept **06**, retained for the actual app/window icon only; it is **not displayed in the Home corner**.
- Windows launcher icon: **exactly the same LO vector inside a centered dark circle**, with transparent outside, no separate logo treatment.
- Muted charcoal, creamy soft blue and dusty rose; no glow, busy grids, decorative micro-labels or invented UI information.
- Four real primary actions: Play, Editor, Skins, Settings. Skins and Settings have more visible tinted treatments; Exit is a small clearly bounded button.
- Home hover now resolves in 55 ms (press 45 ms; focus 95 ms), independently of other menus.

## Files

- `preview.html`: offline code-based home mockup and illustrative controls/transition; open in a browser.
- `wordmark.svg`: full `locus.` mark.
- `monogram.svg`: short `LO` mark.
- `app-icon.svg`: circle-backed, geometrically centered `LO`; transparent outside circle.
- `assets/ui/wordmark.png` and `assets/ui/app-icon.png` (repository-root `assets/ui/`): rasterizations of the approved SVG outlines, bundled next to the Windows executable via CMake for Home and runtime window icon.
- `tokens.json`: shared palette and initial style/motion tokens.
- `IMPLEMENTATION_PLAN.md`: migration architecture, animation rules and review stages.

The mockup does not contain an audio engine, editor logic or real navigation.

## Agreement

New screens first receive a reviewable HTML/CSS/SVG mockup with interaction states, **then** are implemented in C++ after approval. Keep design reference separate from production assets and do not copy HTML rendering into raylib.

No chart, Song Set, SKIN or user-settings file formats were changed by adding these assets.
