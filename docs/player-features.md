# What your AI agent can do in your game — a player's guide

This mod lets an **AI agent you set up** connect to your running Satisfactory
save, **see the world**, and **act in it** — building factories, wiring power,
running trains, and more. You don't need to know how it talks to the game; you
just describe what you want in plain Satisfactory terms ("build me a copper ingot
line by that node", "hook these machines to power", "send a train between my two
stations") and the agent figures out the steps.

This page is the **menu of what's possible** and, just as importantly, **what
actually happens in your world** when the agent does it. It's written for a
pioneer, not a programmer.

> **The golden rule: the agent does real things to your real save.** It spends
> real materials, places real buildings, and can move you around. **Back up any
> save** you let it loose on, and treat that save as modded. Everything powerful
> or "cheaty" is **off by default** (see *Safety* at the end).

---

## How you work with it (in one minute)

- You run your game normally. The agent runs alongside (you set it up once).
- You prompt the agent in game language. It can **look things up live** — it
  always knows your current machines, inventory, power, resource nodes, etc.
- It then performs **explicit, validated actions**, the same way a pioneer with a
  build gun would: with real material costs, real placement rules, and the game's
  own "can't build here" checks.
- It can do one thing or chain hundreds of steps to build a whole factory
  unattended. (Factories *have* been built end-to-end this way — a copper line
  and a full Heavy Modular Frame factory.)

---

## 1. Seeing your world (the agent's "eyes")

Before it builds anything, the agent can read your world in detail. This never
changes anything — it's pure observation. It can see:

- **Resources:** every resource node and deposit (type, purity, whether a miner
  is already on it), and water volumes for water extractors.
- **Your factory:** every building you've placed (with size/footprint), every
  belt/pipe/power connection and whether it's actually hooked up, every machine's
  recipe, clock speed, and input/output inventories.
- **Power:** poles, switches, line limits, and what's powered vs. not.
- **Logistics:** all trains (and whether they're self-driving), train stations,
  freight platforms, trucks, truck stations, drones and drone stations (with fuel
  and cargo).
- **Progression:** HUB milestones and tech tiers, M.A.M. research (including hard
  drives), the Dimensional Depot (central storage) contents, and the Space
  Elevator / Project Assembly status.
- **You:** your position, where you're aiming, and your inventory.
- **The land & hazards:** ground height anywhere, a terrain survey over an area,
  the map's deadly edge zones and gas clouds, and the giant flying creatures.
- **Reference data:** the full catalog of every recipe, item, and buildable in
  the game, and the real material cost of anything before you build it.

*Example prompts:* "What resource nodes are near me and which are free?" ·
"Which of my machines are unpowered?" · "How much iron do I have across all my
storage?"

---

## 2. Building factories

The agent can build essentially anything a pioneer can, and it pays the **real
material cost** out of your inventory/storage (it is not free building unless you
turn on creative mode — see *Safety*).

- **Place & remove buildings:** any machine, foundation, wall, pole, storage
  container, train station — anything in the build menu — and dismantle buildings
  or vehicles.
- **Miners & extractors:** place a miner on a resource node, a water extractor on
  a water volume, or a portable miner (and pick it back up / empty it).
- **Connect everything:** belts, vertical lifts, pipes, hypertubes, and power
  lines — each with a "will this work?" dry-run check first.
- **Structural pieces:** beams and stackable supports for elevated builds.
- **Configure machines:** set a machine's recipe, set its clock speed (overclock),
  install power shards, recolor it, or rotate it.
- **Smart logistics:** set the sort rules on programmable/smart splitters.
- **Know the cost first:** ask what a build will cost before committing.

*Example prompts:* "Build a smelter next to that iron node, feed it with a belt,
and connect it to power." · "Overclock all my constructors to 150% and add
shards." · "Set that refinery to the Diluted Fuel recipe."

> **Heads-up on effects:** when the agent builds or connects things, it briefly
> **turns your view toward the work** (and leaves it pointing there), and for
> **belt connections it may teleport you next to the spot and back** if you're
> far away. Full details in [RPCs and the player](rpc-player-interaction.md) —
> short version: don't be surprised if your camera swings or you hop to the build
> site for a moment.

---

## 3. Power

- See which poles and machines have power and which don't.
- Run power lines between anything (no length limit on the connection).
- Flip power switches on/off and set priority-power-switch groups.

*Example prompt:* "My new factory is dark — find the nearest powered pole and wire
it up."

---

## 4. Logistics — trains, trucks, drones

**Trains:** lay track, place stations and freight platforms, set a train's
timetable (which stations it visits), and switch self-driving on/off. The agent
can build and run a working rail line end-to-end. *(Long, winding rail is the
hardest thing to build well; simple lines and loops are reliable.)*

**Trucks:** build vehicle paths, spawn trucks/tractors/explorers, and arm a
truck's autopilot on a station route (with fuel). It can even tune a vehicle's
top speed.

**Drones:** place drone stations, pair them into a route (both directions), and
load cargo and fuel.

*Example prompts:* "Set up a train that carries steel beams from my steel plant to
the HUB." · "Pair these two drone ports and start them hauling batteries."

---

## 5. Progression — milestones, research, Space Elevator

The agent can push your game forward through the **normal** progression paths
(spending real items):

- **HUB milestones:** pay a milestone off from your inventory (or auto-pull the
  shortfall from the Dimensional Depot), select it as active, and launch it.
- **M.A.M. research:** start research nodes, claim finished research, and handle
  hard-drive alternate-recipe rewards (claim / reroll).
- **Space Elevator / Project Assembly:** pay the next phase and press the upgrade.
- **Dimensional Depot:** upload items into central storage and withdraw them.

*Example prompt:* "Produce what's needed and pay off the next three HUB milestones,
pulling from the Depot if I'm short."

---

## 6. The world around you

- **Creatures:** see them; despawn a troublesome one; (spawning creatures is an
  optional, off-by-default setting).
- **Giant flying mantas:** see, freeze, speed up/slow down, or scrub them along
  their flight path (and spawn a flock) — a creative/visual toy.
- **Hazards:** check whether a spot is inside a deadly map-edge zone or gas cloud,
  and (creative) disable or destroy those volumes.
- **Seasonal events:** force the HUB party mode / holiday events on or off
  (creative).
- **Time & weather of play:** set the in-game time of day.
- **Photos:** enter/exit photo mode, place a free-flying photo camera to frame a
  shot, and take screenshots or high-res photos to disk.
- **Map markers:** drop and remove map markers.
- **Save:** save your game on command.

*Example prompts:* "Set it to midday and take a nice high-res photo of my factory
from above." · "Mark every impure node I haven't mined yet on the map."

---

## 7. What it can do to *you*, the pioneer

A few actions affect your character directly — worth knowing so nothing is a
surprise:

- **Teleport:** it can teleport you to a spot (handy for long builds). Belt builds
  may also hop you to the work and back automatically.
- **Your camera:** building actions turn your view toward the build and **leave it
  there** — expect your aim to move.
- **Your inventory:** (creative only) it can inject items straight into your pack;
  normally it only moves real items you already have.

Two situations where the agent **can't act on you**: while you're **in photo
mode** (building is disabled — it'll just fail until you exit) and while you're
**driving a vehicle** (your character reads as "in the vehicle", so builds and
your position/inventory reads don't work — hop out first). See
[RPCs and the player](rpc-player-interaction.md) for the specifics.

---

## 8. Safety, limits, and the "cheaty" stuff

- **Real saves, real stakes.** Everything is a real write to your live save.
  **Back up first.** This is experimental; treat any save you use with it as a
  modded/creative save.
- **Not achievement-safe.** Even by default it can do things a normal session
  can't (like teleporting you), so don't expect Steam achievements to be "clean"
  on a save you've driven with an agent.
- **Creative/cheat features are OFF by default**, and **only you** can turn them
  on in the mod's settings menu — your agent can never enable them itself. Those
  gated extras include: **injecting free items** into your pack or machines,
  **re-firing milestone achievements**, **jumping the real game phase**, **forcing
  seasonal events**, **moving/disabling hazard zones**, **moving the orbital
  station**, **manta flocks**, and **vehicle speed tuning**. A default install is
  "look at everything + build with real materials like a normal pioneer."
- **Privacy/networking:** by default the agent can only connect from **your own
  PC**. (A separate off-by-default setting can open it to your home network — only
  do that on a network you trust.)
- **Single-player focused.** Multiplayer is largely untested.
- **Honest about rough edges:** some connections can report success but need a
  quick verify (the agent knows to re-check), and very long/twisty rail is
  finicky. Simple-to-moderate builds are reliable; ambitious megastructures may
  need a few tries.

---

## 9. Getting the most out of it (prompting tips)

- **Talk in game terms.** "Build a reinforced iron plate factory", "connect this
  to power", "send a train from A to B" — the agent maps that to the right actions.
- **It already knows your world.** You don't need to tell it coordinates or
  inventory; it can look them up. "the iron node northeast of my HUB" is enough.
- **Ask it to check first.** "Plan it and tell me the material cost before
  building" works — it can dry-run and price things.
- **Let it verify.** For big builds, "build it and then confirm everything's
  powered and producing" plays to its strengths.
- **You stay in control.** It only does what you ask; if you want it to pause,
  save, or undo (dismantle) something, just say so.

---

*New to the mod? Start small — ask the agent to survey your base and suggest the
next thing to automate, then let it build one machine line. Back up your save
first.*
