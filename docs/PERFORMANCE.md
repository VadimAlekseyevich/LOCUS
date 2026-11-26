# Performance V1 — experimental local PP

Performance estimates physical execution of an individual completed attempt.
It is distinct from chart stars (Difficulty V4), judgement rules and future
server ranking eligibility. All constants are provisional, not fitted to
player data.

For a completed run, let S be the effective physical star rating, N the
number of physical key judgements (two for a diagonal), A the weighted
accuracy as a fraction from zero to one, and M the number of missed keys.

```text
PP = 12 * S^2.2 * A^6 * (1 - M/N)^2 * min(1, sqrt(N/200)) * E
E = 0.65 with EZ, otherwise 1
```

HT/DT recalculate physical stars at the actual fixed rate; RD applies its
recorded deterministic seed to the source chart; MR preserves physical
strain. HD, FL, HR, SD and PF receive no direct multiplier in this version.
EZ widens windows and receives a provisional penalty. READ, route geometry,
skin, combo and UR do not add PP. The short-chart factor saturates at 200
physical presses; the difficulty estimator itself still measures sustained
strain. All-miss results yield zero. Partial, assisted, failed and
unsupported-rule attempts do not qualify; the built-in demo is excluded
from local PP. No online ranking policy has been selected.

The portable core calculator accepts authoritative chart and score data.
The desktop saves optional `performanceDifficulty` inputs (difficulty
version, effective stars, physical key count) inside Results v3, captured
once at completion, outside the per-frame judging path. PP is derived, not
stored as a permanent score. Native Results displays local V1 PP when the
inputs are supported. Existing v3 files without this optional field remain
readable but unrated. If difficulty versions change, recomputation requires
the exact chart revision and timing; do not silently reinterpret old stars.
A server must calculate these inputs from its own published chart and
verified replay, never accept a client's snapshot as authoritative.

The pure profile projection `SummarizePlayerPerformance` accepts one owner
and a collection of attempts. It reports play count, physical-key-weighted
accuracy across valid unique attempts, and the sum of the best PP from each
stable set/difficulty key, sorted descending and weighted by 0.95^i
(zero-based). Rates, mods and old authored revisions compete for the same
slot. Other owners and conflicting repeated attempt IDs are excluded; identical
projection inputs count once. No extra bonus for retries or play count.
The short-chart confidence factor can increase with extra notes below its
200-key saturation point; test padding sensitivity during calibration. This projection
does not yet scan the complete on-disk history or expose a profile screen.
Local profile policy is not the final competitive leaderboard policy.

Next: collect representative charts and real human runs, calibrate PP and
mod eligibility, then connect the owner-scoped history reader and profile UI.
