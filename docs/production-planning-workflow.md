# Production planning workflow (end-to-end)

How to go from "I want N of part X per minute" to a built, running factory, using
the deterministic Python tools plus the RPC. Each step names the tool, what it
answers, and the DECISION the agent makes (the tools compute and expose; they do
not choose plans/layouts/sites — see [[feedback_dont_prebake_agent_decisions]]).

All Python runs from `controller/`. The recipe/siting tools work OFFLINE against
`catalog_cache.json`; refresh it after a game/mod patch with
`python export_catalog.py` (game must be running).

---

## 1. Bill of materials — what to build  → `satisfactory_ai/recipe_tree.py`
Given target part + rate, get every intermediate recipe, machine counts (+clock%),
raw-resource rates, byproducts, machine totals, and total power.

    python -m satisfactory_ai.recipe_tree "Reinforced Iron Plate" 10

Decisions you make here (the solver forces them rather than guessing):
- **Alternate / ambiguous recipes** → it raises `RecipeChoiceNeeded`; re-run with
  `--choose Desc_X_C=Recipe_Y_C` (or `recipe_choices={}` in code).
- **What to import vs make** → `--raw "Iron Ingot"` treats a part as a sourced
  leaf. `/RawResources/` (ore, water, ...) are leaves by default.

Take from the result: `raw_totals` (feeds step 2), machine counts + `total_power_mw`
(feeds steps 3–4), byproducts (need a sink — [[feedback_byproduct_sink]]).

## 2. Where to build — resource siting  → `satisfactory_ai/siting.py`
Feed `world.resourceNodes` telemetry + the BOM's `raw_totals` in:

    rank_sites_for_demand(nodes, bom.raw_totals, link_radius=..., miner_mk=...)

Returns clusters (candidate sites, with centroid) ranked by whether their
extraction (`extraction_rate` = purity × miner Mk) meets the demand, with
per-resource deficits. Decisions: which site, which miner tier, whether a partial
site + belt/train imports for the deficits is acceptable. (Solid nodes only;
water/oil pumped separately.) Pick a build centroid from the chosen cluster.

## 3. Footprint & layout  → `satisfactory_ai/layout.py` + `composites.py`
You now know machine counts (step 1) and a site (step 2). Use the geometry
toolkit to place them:
- foundation footprint: `compute_disk_fill_grid` / `compute_outer_touching_ring`
  (size from machine totals; default 8x1 foundations — [[feedback_prefer_1m_foundations]]).
- machine rows + manifolds: `composites.machine_row`, `composites.manifold`,
  `vertical_pair_block`; power backbone: `pole_backbone`.
- connector geometry (does this belt pair fit? where to place to align?):
  `predict_connector_world_state`, `compute_aligned_placement_position`,
  `connectors_are_compatible`, `candidate_yaws_for_normal`.
Pre-plan compactly, elevated/clear terrain, belts routed intentionally
([[feedback_build_layout_preplanning]]).

## 4. Belts / pipes  → `satisfactory_ai/router.py` (+ conveyors.py / pipes.py)
`router` turns "connect output A → input B" into a concrete belt plan (direct, or
segments + jog-merger/relay/elevation) honoring the measured belt rulebook
(min run, `maxSplineLength`, 30° incline, S-against-facing). For long hauls from
the resource site, cross hostile terrain with a lift-skyway, teleport the player
NEAR the work (belt validation is camera-dependent) — [[reference_belt_haul_terrain_rules]].

## 5. Power
`total_power_mw` from step 1 tells you the generator/fuel need. Wire with
`connectPower` (no length limit; `ignoreAimLocation`); if it wedges globally,
a full game restart clears it (project_hmf_optimization bug #2).

## 6. Build & verify  → `satisfactory_ai/executor.py` + `world.batch`
Execute placements/connections via the executor (auto-repairs dangling belts,
best-effort teleports) and `world.batch` (≤100 ops). Then verify real positions
with `world.buildables` before trusting success ([[feedback_validate_test_placements]]),
confirm machines actually produce (`setClockSpeed` is pending until a machine
runs), and give byproducts a sink. Save at natural pauses
([[feedback_session_saving_discipline]]).

---

### Boundary reminder
The tools answer questions and compute numbers deterministically (CLAUDE.md's
"deterministic solver responsibilities"); the agent decides between valid plans,
alternates, sites, and objectives (CLAUDE.md's "LLM responsibilities"). Keep it
that way — no tool here should silently pick a layout, recipe, or site.
