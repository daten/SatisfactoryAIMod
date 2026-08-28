# DocMod Playbook — Requesting Builds in Plain Language

This file is for **playing** the game with DocMod's AI interface, not developing
the mod. If you're working on DocMod's own C++/Python source, see `CLAUDE.md` /
`AGENTS.md` instead.

This is a first draft, meant to be edited. Nothing here is enforced by code —
it's a shared vocabulary so your requests get interpreted the way you expect.
Copy it into your own save/profile notes and adjust the defaults, examples, and
keyword list to match how you actually play.

---

## The default contract

Unless you say otherwise, "build me X" or "plan X" means:

- **Default recipes only.** Alternate recipes are not substituted automatically
  — you haven't necessarily unlocked the ones that matter, and "better" is
  situational (fewer machines vs. fewer raw inputs vs. less power aren't the
  same axis).
- **No Power Shards, no Somersloops, no overclocking.** Every machine runs at
  100% clock, stock.
- **No Resource Converters.** Raw resources (ore, coal, oil, etc.) come from
  real extraction, not late-game resource-conversion recipes that turn one raw
  resource into another — those change what "raw" even means for the chain and
  should be opt-in.
- **Every tier sized to avoid starving the next one.** Machine counts are
  computed from real recipe rates and rounded UP, never down — a slight
  surplus at each tier is expected and correct, not a mistake.
- **Standard building tiers** (Mk.1 where a choice exists) unless the target
  rate genuinely requires a higher tier, or you ask for a specific one.

If you want something other than this, say so — see the modifier keywords
below. When in doubt, the agent should tell you what it assumed before
building anything large, not guess silently.

## Modifier keywords

These aren't magic words the code parses — they're a shared vocabulary so a
plain-English request reads unambiguously. Mix and match freely.

| Say this | Means |
|---|---|
| "using alternates" / "best alternate recipes" | Substitute alternate recipes where they reduce machine count or simplify inputs. Say "only alternates I've unlocked" if that matters — the agent can't always tell what's actually unlocked vs. theoretically known. |
| "with sloops" / "socket Somersloops" | Slot Somersloops in Augmenter-capable buildings to roughly double output per machine (raises power draw). Say how many you actually have — sloops are a limited resource. |
| "with power shards" / "overclock to N%" | Use Power Shards to overclock machines above 100%, reducing machine count for a given rate. Say a target % or let the agent pick one that gives a clean machine count. |
| "using resource converters" | Allow late-game raw-resource conversion recipes (e.g. Iron Ore from Limestone) as a source, not just extraction. Changes what counts as a "raw" bottleneck. |
| "minimal footprint" / "fewest machines" | Optimize for machine count over raw-resource efficiency — the opposite bias from the default, which optimizes for using the cheapest (default) recipes even if that means more machines. |
| "don't touch my existing X" | Treat an existing factory/line as fixed capacity — build around it, don't resize or reroute it. |

## Transport keywords (for anything that has to move between places)

Default: **belts and pipes** for anything reasonably close (the same general
area, or within a distance you'd sanely walk). For longer runs, say which you
prefer, or let the agent recommend one based on distance and volume:

| Option | Good for | Tradeoff |
|---|---|---|
| Belts / pipes | Short-medium distance, steady flow | Cheap, simple, but terrain-following gets unreliable over long/rough distances (see the elevated-platform pattern in project notes) — and it's a lot of individual pieces to place for a long run. |
| Trains | Long-distance bulk transport, high throughput | Needs rail infrastructure (and ideally an elevated bed — see above) and a power connection, but scales well and is the most bandwidth-efficient long-haul option. |
| Trucks/vehicles | Medium distance, low-to-moderate throughput | Minimal infrastructure (a rough road at most), but AI pathing can be unreliable for anything precise or automated. |
| Drones | Point-to-point over any terrain, including obstacles belts/rail can't cross | No infrastructure between endpoints, but lower throughput and needs power + fuel/battery at both ends. |

Say "prefer trains" / "use drones for this" / "belts are fine" to lock in a
choice; otherwise expect a recommendation with the reasoning stated, not a
silent pick.

---

## The four request shapes

These aren't rigid categories — they're a spectrum, and which one applies is
mostly determined by what the agent finds when it actually looks at your
current save, not by which case you name. State your goal in plain language;
mentioning roughly which of these fits helps the agent scope the read-before-write
investigation correctly, but isn't required.

### 1. New factory for a resource, established late-game base already exists

**Depth: shallow.** Most of what's needed (power, some raw supply, transport
options) probably already exists somewhere reachable. The real work is
figuring out what's already available with spare capacity vs. what's
genuinely new, not computing a chain from zero.

What should happen: read the existing world state first — nearby unclaimed
resource nodes, existing power grid reach and headroom, existing intermediate
products already in production with spare output — before computing anything
new. The plan should say explicitly what it's reusing vs. building fresh.

Example: *"Build a new Heavy Modular Frame factory near my main base,
optimized, reuse my existing steel supply if it has spare capacity, extract
fresh iron if not."*

### 2. Extend, expand, or scale an existing factory

**Depth: medium — this is a delta calculation, not a from-scratch one.** The
agent needs to read the *current* factory's real machine counts and
production status first, compute the gap between current and target output,
and figure out which upstream tiers need more capacity too (scaling the top
of a chain without scaling its inputs just moves the bottleneck down one
level).

What should happen: telemetry read of the actual current state before any
math, a stated current-vs-target comparison, and a check for whether any
upstream tier already has slack before adding more of it.

Example: *"Double my current Heavy Modular Frame output"* or *"my new
factory from the last request needs more copper wire — scale that line to
keep up."*

### 3. Supporting infrastructure — power plants and their own dependencies

**Depth: medium-deep.** Power generation has its own dependency chain (fuel
supply, and for some generator types, water/cooling) that's structurally the
same kind of problem as a production chain — it just terminates in MW instead
of items/min.

What should happen: determine the required MW (either stated directly, or
summed from whatever else is being built in the same request), pick a
generator type (say if you have one in mind — coal, fuel, nuclear, and
geothermal all have real tradeoffs in fuel logistics, waste, and footprint),
then chain the fuel requirement back through its own production or extraction
the same way any other input would be resolved.

Example: *"Build a coal power plant sized to run the Heavy Modular Frame
factory from before, including its coal supply."*

### 4. Extreme case — build a resource from scratch, nothing exists, inventory empty

**Depth: deepest.** Full recursive chain from raw extraction through every
intermediate up to the target, *plus* power generation for every machine
along the way (which has its own chain per case 3), assuming zero existing
infrastructure and zero starting inventory. This is close to "plan the whole
early-to-mid game for one goal" in a single request.

What should happen, specifically because this can spiral to a very large
plan: the agent should compute and report the **full scope — total machine
count, total power draw, raw extraction rates needed** — before starting to
build anything, the same way a large purchase gets confirmed before it
happens. A hundred-plus-machine plan is fully reversible (everything can be
dismantled) but expensive in time to execute and to undo, so it's worth a
look before committing, not just before the biggest individual actions
within it.

Example: *"Assume nothing is built and my inventory is empty — plan and
build everything needed to produce Heavy Modular Frames from scratch,
including power."*

---

## Worked example: Heavy Modular Frames, case 4 shape, default assumptions

For "build Heavy Modular Frames from scratch, optimized, default recipes, no
sloops/shards/overclock" — the actual computed answer (from a real live
recipe-catalog query, not estimated) was:

| Product | Building | Count |
|---|---|---:|
| Heavy Modular Frame | Manufacturer | 1 |
| Modular Frame | Assembler | 5 |
| Encased Industrial Beam | Assembler | 2 |
| Steel Pipe | Constructor | 2 |
| Screws | Constructor | 11 |
| Reinforced Iron Plate | Assembler | 3 |
| Steel Beam | Constructor | 3 |
| Concrete | Constructor | 5 |
| Iron Plate | Constructor | 5 |
| Iron Rod | Constructor | 12 |
| Steel Ingot | Foundry | 6 |
| Iron Ingot | Smelter | 11 |

Raw extraction needed: Iron Ore 600/min, Coal 270/min, Limestone 225/min —
66 machines total, before adding power generation for all of them (which a
real case-4 request would also compute and include).

This is what "default optimized" looks like in practice — a good reference
point for calibrating your own requests.

---

## Notes for whoever edits this file

- This is a starting point, not a spec. If your play style always wants
  sloops by default, or you never use trains, change the defaults above
  instead of typing the modifier every time.
- Worked examples go stale as recipes/mechanics change between game versions
  — treat the numbers as illustrative, not authoritative, and regenerate them
  against a live save if precision matters.
- This file is intentionally agent-agnostic — it describes a request
  vocabulary, not an API. It should read the same whether you're talking to
  Claude, ChatGPT, Grok, or anything else driving the same RPC interface.
