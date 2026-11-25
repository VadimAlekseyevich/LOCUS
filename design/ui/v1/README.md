# LOCUS — brand source and visual reference

The approved logo and icon geometry is maintained here. The offline [Nightbloom
prototype](../v2/index.html) illustrates a menu design; production behavior is
defined by [UI architecture](../../../docs/UI_RML_MIGRATION.md) and
[game design](../../../docs/GDD.md).

- Lowercase **locus.** wordmark with sky-blue dot (concept 02).
- Compact **LO** (concept 06) is for the app/window icon, not a duplicated
  Home corner badge. The icon is centered in a dark circle with transparent outside.
- Dark violet-blue palette, sky accents, restrained lilac emphasis. Four
  primary Home actions (Play, Edit, Skin, Settings) and a small Exit button.

Files: `wordmark.svg`, `monogram.svg`, `app-icon.svg`, `tokens.json`,
`preview.html` (illustrative offline Home) and `render_brand.py`.
The runtime consumes rasterizations under repository-root `assets/ui/`,
not the HTML mockup. Regenerate after an approved vector/palette change:

```sh
python -m pip install cairosvg Pillow
python design/ui/v1/render_brand.py
```

Keep design and production assets separate. Review new visual design before
implementation; reuse the shared native component foundation. See
[design boundaries](IMPLEMENTATION_PLAN.md).
