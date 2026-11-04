# LOCUS — Visual Wrap Seam Contract

**Task:** A-04 — Specify visual Wrap seams  
**Status:** Proposed presentation contract; awaiting Stage 1 owner review and later gameplay testing  
**Scope:** 4×4 matrix, four cardinal Wrap directions, turns at edge points, marker and route preview  
**References:** [GDD](../GDD.md), [Timing](./TIMING.md), [Boundaries](./BOUNDARIES.md), [Roadmap](../ROADMAP.md)

> **Confirmed gameplay rule:** The MVP has **Wrap only**. A step across a border re-enters on the opposite side, preserves heading, lasts one quarter-note beat, and requires no extra input or judgement. The treatment below is the **proposed visual contract**, not a new movement mechanic.

## 1. Invariants

1. **Never draw a continuous line through the interior** between opposite grid points.
2. Display a Wrap as two **disconnected outer-edge pieces**: exit and entrance, aligned on the same row or column.
3. The combined movement occupies **exactly one musical beat**, just like an ordinary step.
4. Show **one player marker** at every instant, never simultaneous copies on both sides.
5. Remove the already traversed route immediately behind the marker.
6. Player success, wrong input, or Miss never affects the authored route or its preview.
7. Graphics sample the authoritative route at musical time; no independent frame-based Wrap clock exists.

## 2. Grid-space geometry

Use the existing logical coordinates: x and y from 0 through 3, with x rightward and y downward. Adjacent grid point centers are one **grid unit** apart; the renderer transforms those units into pixels.

The visible area extends **half a grid spacing** beyond the outermost point centers:

~~~text
LEFT seam    x = -0.5          RIGHT seam   x = 3.5
TOP seam     y = -0.5          BOTTOM seam  y = 3.5
~~~

Seams are visual border positions, **not** playable cells. Leave space for the player marker and stroke at these edges.

For a Wrap step:

- **S:** logical source grid point.
- **D:** destination grid point after modulo-4 Wrap.
- **v:** cardinal unit direction vector, Right=(1,0), Left=(-1,0), Up=(0,-1), Down=(0,1).
- **p:** normalized musical progress from 0 through 1 during this **one beat**.

### 2.1 Marker visual position

~~~text
if 0 <= p < 0.5:    marker = S + p * v
if 0.5 <= p <= 1:   marker = D - (1 - p) * v
~~~

At p=0 the marker is at S. Immediately before p=0.5 it approaches the outgoing seam. **At exactly p=0.5 it appears on the incoming seam**; it does not exist simultaneously on the other side. At p=1 it arrives at D.

This midpoint discontinuity is a **screen-space teleport between edges**, not a discontinuity in audio time. Normal non-Wrap steps simply use S + p*v for the whole step.

### 2.2 Future path pieces

For a fully visible Wrap step draw these independently:

~~~text
EXIT piece:   S -> S + 0.5*v
ENTRY piece:  D - 0.5*v -> D
~~~

**Never connect the two pieces.** Each is half a grid spacing long, together representing **one beat**, not two steps. Clip the two pieces independently when the preview front or marker lies inside the step.

## 3. Annotated horizontal Wraps

### H-01 — Right edge to left edge

**Logical movement:** (3,1) with heading Right moves to (0,1); heading stays Right.

~~~text
EXIT, row y=1:
    (0,1)   (1,1)   (2,1)   (3,1) ----> | right seam x=3.5
                                 source    exit

ENTRY, same row:
    left seam x=-0.5 | ----> (0,1)   (1,1)   (2,1)   (3,1)
                       entry   target

Never connect (3,1) to (0,1) through the board center.
~~~

| Progress p | Display x | Meaning |
| ---: | ---: | --- |
| 0.00 | 3.00 | At source |
| 0.25 | 3.25 | Moving toward right seam |
| 0.50 | -0.50 | Reappears at left seam |
| 0.75 | -0.25 | Moving toward target |
| 1.00 | 0.00 | At destination |

y remains 1. As p approaches 0.5 from below, x approaches 3.5; on p=0.5 it changes to -0.5.

### H-02 — Left edge to right edge

**Logical movement:** (0,2) with heading Left moves to (3,2); heading stays Left.

~~~text
EXIT, row y=2:
    left seam x=-0.5 | <---- (0,2)   (1,2)   (2,2)   (3,2)
                       exit   source

ENTRY, same row:
    (0,2)   (1,2)   (2,2)   (3,2) <---- | right seam x=3.5
                                target    entry
~~~

x moves 0 to -0.5 before the seam, then 3.5 to 3 after the seam. Both portions visibly move **Left**, never through the grid center.

## 4. Annotated vertical Wraps

### V-01 — Top edge to bottom edge

**Logical movement:** (2,0) with heading Up moves to (2,3); heading stays Up.

~~~text
   TOP seam y=-0.5             ^ exit
                               |
   row 0               (2,0) source
   row 1               (2,1)
   row 2               (2,2)
   row 3               (2,3) target
                               ^
   BOTTOM seam y=3.5           | entry

Draw ONLY (2,0) -> (2,-0.5) and (2,3.5) -> (2,3).
~~~

y moves 0 to -0.5, then 3.5 to 3. Do not connect through rows 1 and 2.

### V-02 — Bottom edge to top edge

**Logical movement:** (1,3) with heading Down moves to (1,0); heading stays Down.

~~~text
   TOP seam y=-0.5             | entry
                               v
   row 0               (1,0) target
   row 1               (1,1)
   row 2               (1,2)
   row 3               (1,3) source
                               |
   BOTTOM seam y=3.5           v exit

Draw ONLY (1,3) -> (1,3.5) and (1,-0.5) -> (1,0).
~~~

y moves 3 to 3.5, then -0.5 to 0. Do not connect through rows 1 and 2.

## 5. Turn-on-edge and corner examples

**Authoritative order from TIMING.md:** arrive at a point on beat n, apply any scheduled relative turn, then move one step with Wrap if the **outgoing** direction crosses the border.

### E-01 — A corner turn creates a vertical Wrap

- At beat 0, the marker starts at **(3,0)** with heading **Right**.
- Event at beat 0: **Left**. Outgoing heading becomes **Up**.
- The step moves **(3,0) to (3,3)** through the **top/bottom seam**.
- No rightward Wrap to (0,0) occurs. At beat 1 the marker is at (3,3).
- A wrong or missed Left press changes only the judgement, not this route.

~~~text
 top seam y=-0.5                         ^ exit
                                         |
 y=0     (0,0)  (1,0)  (2,0)  (3,0)  START
 y=1     (0,1)  (1,1)  (2,1)  (3,1)
 y=2     (0,2)  (1,2)  (2,2)  (3,2)
 y=3     (0,3)  (1,3)  (2,3)  (3,3)  TARGET
                                         ^
 bottom seam y=3.5                       | entry

 Two separate vertical outer-edge pieces, no central chord.
~~~

### E-02 — A corner turn prevents Wrap

- At **(3,0)** with initial heading Right, event **Right** changes heading to **Down**.
- The outgoing step is **(3,0) to (3,1)**, an ordinary adjacent move. No seam or jump should be drawn.

### E-03 — An edge turn changes the Wrap axis

- At **(0,0)** with incoming heading Up, event **Left** makes the outgoing heading **Left**.
- The step is **(0,0) to (3,0)** using the **left/right seam**.
- A top/bottom preview would be incorrect: seam choice is based on heading **after** the charted turn.

## 6. Route preview reveal and instant removal

**Confirmed preview behavior:** The author configures a look-ahead **L** in beats. At current chart beat **b**, the preview shows the correct future route from current marker progress to **b+L**, clipped at the end of the audio. Past route disappears immediately. The preview is always determined by the chart, not player's inputs.

**Proposed geometry algorithm:**

1. Request **ordered** route segments from the shared Route sampler for the future interval from **max(0,b)** to **min(b+L, song-end beat)**.
2. Clip the first segment to the marker's exact normalized progress, and the final segment to the preview's current leading progress.
3. For an ordinary segment, draw the clipped adjacent-point line.
4. For a Wrap segment whose visible normalized interval is [a,z], intersect with **[0,0.5]** for the exit and with **[0.5,1]** for the entrance. Draw each nonempty piece **separately** using Section 2 geometry.
5. A seam entry piece is invisible until the **preview front** has reached the second half of that Wrap. A past exit piece vanishes as soon as the marker passes it.
6. Never combine steps solely because they revisit the same cell. Preserve musical order. Styling ambiguous overlaps is deferred to R-11 and P-08.

**H-01 preview checkpoints:**

- **Preview front at p=0.25:** show only the exit piece from x=3 to x=3.25.
- **Marker at p=0.25, front beyond p=1:** show x=3.25 to 3.5 on the right **and separately** x=-0.5 to 0 on the left.
- **Marker at p=0.75, front beyond p=1:** show x=-0.25 to 0 on the left; nothing remains on the right.
- **Marker at p=0.5:** show one marker at x=-0.5; only the incoming route is ahead.

A Wrap is still **one beat** in every case. No extra event or judgement is introduced by a seam.

## 7. Style and unresolved cosmetic details

**Proposed prototype defaults:** Use one muted pastel future-path color consistently on both sides of a seam. Optionally show matching tiny edge ticks or subtle arrows at the corresponding exit and entrance, **without an interior connecting line**. Keep motion smooth except for the discrete seam switch, avoid flashes and persistent trails, and leave padding for the marker at outer seams.

**Open until playtesting:** Whether seam ticks or a soft pulse are helpful, ideal line thickness, legibility at small resolutions, and styling of overlapping future route segments. Any cosmetic cue must be disableable without losing functional route information. None may shift the beat midpoint, input timestamp, or authored path.

## 8. Acceptance scenarios for implementation

| ID | Setup | Logical outcome | Visual requirement |
| --- | --- | --- | --- |
| W-01 | (3,1), Right | (0,1), Right | Right exit, left entry; no interior line |
| W-02 | (0,2), Left | (3,2), Left | Left exit, right entry |
| W-03 | (2,0), Up | (2,3), Up | Top exit, bottom entry |
| W-04 | (1,3), Down | (1,0), Down | Bottom exit, top entry |
| W-05 | (3,0), Right, Left event | (3,3), Up | Turn before step, vertical seam |
| W-06 | (3,0), Right, Right event | (3,1), Down | No seam; ordinary movement |
| W-07 | (0,0), Up, Left event | (3,0), Left | Horizontal seam despite incoming Up |
| W-08 | Wrap p=0 / .25 / .5 / .75 / 1 | One beat | Correct two-part marker progression, no duplication |
| W-09 | Preview front before Wrap midpoint | Same route | No early entrance stub |
| W-10 | Marker after Wrap midpoint | Same route | No remaining exit-side trail |
| W-11 | Miss or wrong key at edge turn | Same authored route | Same Wrap and preview |
| W-12 | Route revisits/crosses a point | Ordered route samples | No erroneous seam connection |

**Where tests belong later:** R-02/R-03 (logical route), R-06/R-08 (seam geometry), R-07 (preview clipping), P-04/P-06/P-07 (actual visuals and instant trail removal), P-08 (overlaps). The Stage 6 human gate checks readability and feel.

These are **future tests**, not evidence of implemented code or a running renderer.

## 9. A-04 deliverable checklist

- [x] Horizontal Right/Left and vertical Up/Down exits and entries are annotated.
- [x] Edge turns that cause, avoid, or redirect a Wrap are annotated.
- [x] Marker progress on a seam is deterministic, including exactly p=0.5.
- [x] No diagram includes an interior edge-to-edge connector.
- [x] Preview lead and instant removal of traversed pieces are specified.
- [x] Confirmed gameplay and proposed visual details are clearly distinguished.

**Output of A-04:** This document alone. No gameplay/audio/editor/rendering code is added by this task.
