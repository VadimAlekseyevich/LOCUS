# LOCUS UI v2 — design review

- [ ] Home: no corner monogram; clear Play / Editor / Skins / Settings / Exit.
- [ ] Song Library: song tree, difficulty selection, compact chart actions and difficulty breakdown.
- [ ] Selected chart: **Results history appears on the left**, sorted newest first, with date and accuracy. No results if none exist.
- [ ] Opening any history entry shows that attempt's accuracy, Perfect, Good, Bad, Miss and notes judged.
- [ ] PLAY sample: **Finish chart** creates another in-memory attempt and opens its Results detail. Retry and leaving do not create records.
- [ ] Switching difficulty cannot show another chart's attempt history.
- [ ] **Listen and global Results navigation absent.** Results detail is contextual.
- [ ] Settings: Game / Display / Key bindings / Audio & offset, calibrated offset controls.
- [ ] Skin: PATH / PLAYER / COLORS / MY SKINS, real game settings and a preview.
- [ ] Editor: SET / ROUTE / AUDIO / FIELD, one timeline and one whole-song overview, large note pad, up to two reference panes.
- [ ] Dialogs: unsaved, delete, quit, compare and errors keep essential warnings.
- [ ] No instructions beneath obvious buttons, no decorative captions, no redundant headlines.
- [ ] Layout works at 960×540 and 1280×720; mouse, keyboard and focus states remain aligned.
- [ ] No new animation or gameplay effect during PLAY.
- [ ] Preview actions never persist fake results to game/user files.

Native Results recording is documented as a future requirement in [README](README.md); it is **not** implemented by this HTML work.
