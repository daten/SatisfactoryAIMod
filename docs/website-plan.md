# SatisfactoryAIMod promotional website — plan

Status: **PLAN / draft for review** (2026-10-01). Domain not finalized. No code yet.

A static, multi-page site that introduces the mod to the Satisfactory community,
gets players installed, gets them talking to an AI agent (even if they have never
installed agent software), and teaches through simple → medium → complex
tutorials. No server-side code, no database.

---

## 1. Goals & success criteria

| Goal | We know it works when… |
|---|---|
| **Introduce** the mod in game terms | A player understands "my AI agent can see my save and build in it" within 10 s of landing on the home page. |
| **Install** | A player with Satisfactory Mod Manager experience is installed in < 5 min; a total newcomer in < 15 min. |
| **Connect an agent** (incl. first-timers) | Someone who has never installed agent software has their agent read their world (`what resource nodes are near me?`) in < 30 min, following one guide top-to-bottom. |
| **Teach** | Each tutorial ends with a verifiable in-game result (the thing is built *and* producing / driving). |
| **Set expectations** | Nobody is surprised by teleports, camera swings, achievement impact, or real material spend — it's stated before they hit it. |
| **Find** | Ranks for "Satisfactory AI mod", "Satisfactory AI agent", linked from ficsit.app page, Reddit, modding Discord. |

## 2. Audiences & journeys

1. **Curious player** (most visitors) — Home → Features → Showcase → *maybe* Install.
   Needs: wow factor, game language, trust/safety.
2. **Ready-to-try player, new to AI agents** — Install → *Choose your agent* → one
   step-by-step agent guide → *Your first conversation* → Simple tutorial.
   Needs: zero jargon, screenshots of every click, a copy-paste starter kit.
3. **Player already using an AI coding agent** — Install → *Starter kit* →
   Tutorials (medium/complex). Needs: brevity, the endpoint, good prompts.
4. **Tinkerer / developer** — Developers page → GitHub, RPC catalog, Python client.
   Kept off the main player path so it doesn't intimidate group 1–2.

## 3. Facts & constraints that shape the site

- **Not yet on ficsit.app.** Remaining release items (from the release checklist):
  create the ficsit.app page, mirror the safety disclosure, screenshots, tags,
  upload the `.smod`. → The site should **launch together with the ficsit.app
  listing**, and the screenshots we capture serve both.
- **How agents connect today:** a loopback-only HTTP JSON endpoint inside the
  game (`127.0.0.1:51902/rpc`) that self-describes via `world.help`, plus an
  optional Python client (`controller/satisfactory_ai`). **There is no MCP server.**
  So a supported agent must be able to **run commands or make local web requests**
  (agentic coding tools: Claude Code, Codex, Gemini CLI, Cursor, Copilot agent
  mode, Cline…). **Chat-only websites (chatgpt.com, claude.ai, gemini.google.com)
  cannot reach your game** — the site must say this plainly, early.
- **Safety defaults** to communicate: loopback-only; *Allow Creative Features*
  off (item injection, achievement re-fire, phase skip, hazards/manta/event
  tools, vehicle speed); *Allow Remote Connections* off; real material cost;
  not achievement-safe; teleports + camera swings (see
  [rpc-player-interaction.md](rpc-player-interaction.md)); builds fail in photo
  mode and while driving.
- **Existing content to reuse (single-source it, don't fork):**
  [player-features.md](player-features.md) (game-terms feature list),
  [rpc-player-interaction.md](rpc-player-interaction.md),
  [known-limitations.md](known-limitations.md), the README safety section,
  [rpc-catalog.md](rpc-catalog.md) (generated), factory/vehicle placement guides.
- **Imagery today:** only `Icon128.png`. Everything else must be captured — but
  the mod can photograph itself (§9).
- **Static only:** HTML/CSS/JS output; client-side JS allowed; no backend, no DB.

## 4. Sitemap

```
/                      Home (landing)
/features/             What your agent can do (from player-features.md)
/showcase/             Gallery: real agent-built factories, trains, art
/install/              Install the mod (Mod Manager path + manual/pre-release path)
/agents/               Choose your AI agent (comparison + "what's an agent?")
  /agents/claude-code/     Golden-path guide (fully screenshotted)
  /agents/codex/           Alternate guide
  /agents/gemini-cli/      Alternate guide
  /agents/cursor/          Alternate guide (IDE users)
  /agents/other/           "Any agent that can run commands" generic guide
/starter-kit/          Download + copy-paste agent instructions & first prompts
/first-conversation/   Your first 10 minutes with the agent (guided)
/tutorials/            Tutorial index (filter: Simple / Medium / Complex)
  /tutorials/<slug>/       One page per tutorial
/safety/               Safety, saves, achievements, what it does to your character
/faq/                  FAQ + troubleshooting
/developers/           For tinkerers: GitHub, RPC catalog, Python client, contributing
/changelog/            (optional) release notes / news
```

Global nav (player-first): **Features · Showcase · Install · Agents · Tutorials · Safety** + a
persistent **Get started** button. Developers lives in the footer.

## 5. Page content outlines

### Home
1. **Hero:** headline in game terms — e.g. *"Hire an AI pioneer for your factory."*
   Sub: *"Connect an AI agent to your Satisfactory save. Ask in plain words — it
   surveys, builds, wires, and runs trains with real materials."* Hero visual:
   looping clip/still of an agent-built factory (§9). CTAs: **Get started** ·
   **See what it built**.
2. **Prompt → result strip** (signature component, §8): 3 cards pairing a plain
   prompt with the in-game screenshot it produced (survey / factory / train).
3. **How it works** (3-step diagram, no jargon): *Your game (with the mod)* ⇄
   *Your AI agent on your PC* ⇄ *You, chatting with it.* One line: "The mod is the
   agent's hands and eyes; your agent is the brain."
4. **Feature highlights** (6 tiles → /features): Survey · Build & wire · Power ·
   Trains/trucks/drones · Progression · Photos.
5. **Showcase teaser** (3–4 best shots → /showcase).
6. **Safety callout** (short, honest): real saves, back up, not achievement-safe,
   cheats off by default, only your PC can connect. → /safety
7. **Which agents work** (logo-less text chips + "needs an agent that can run on
   your PC") → /agents
8. Footer: GitHub, ficsit.app page, license (GPLv3), fan-project disclaimer (§12).

### Features
Adapted from `player-features.md` (keep it the source of truth; the page renders it
or a trimmed copy). Add one screenshot per section and example-prompt chips.

### Showcase
Masonry gallery + lightbox. Each item: title, the prompt(s) used, what the agent did,
build time, a "try it" link to the matching tutorial. Launch set: HMF factory,
copper line, drivable perimeter train loop, drone route, truck autopilot, floating
belt tornado / basket weave, photo-mode cinematic.

### Install
- Before you start: back up saves (with exact save-folder path on Windows/Steam &
  Epic), game version requirement, SML dependency (Mod Manager handles it).
- **Path A – Satisfactory Mod Manager** (post-ficsit.app launch): install SMM →
  search "SatisfactoryAIMod" → install → launch. Screenshot per step.
- **Path B – manual / pre-release** (until listing is live, and for testers).
- **Mod settings** screenshot: where the toggles live, what each does, why they're off.
- **Is it working?** Non-technical check: "Your agent will confirm it — see
  /first-conversation". (Optional advanced: open the given URL… — keep in a
  collapsible for the curious.)
- Uninstall / disable.

### Choose your AI agent (/agents)
- **What is an AI agent (vs a chatbot)?** One paragraph + diagram: a chatbot
  only talks; an *agent* runs on your PC and can take actions — that's what
  reaches your game.
- **Comparison table** (verify all rows at write time — products change fast):
  agent · free tier? · needs an account/subscription · runs as app / terminal /
  IDE · difficulty for first-timers · tested by us ✓.
- **Recommendation:** one golden path for first-timers (§6), alternates for people
  who already have a preference.
- **Won't work:** chat websites/mobile apps that can't run commands on your PC
  (unless/until an MCP connector exists — §6, §15).

### Agent guide (one per agent, same template)
1. What you need (account, cost, OS).
2. Install (screenshot per click).
3. Sign in.
4. Make a "Satisfactory agent" folder and drop in the **starter kit** file.
5. Start the agent in that folder.
6. Paste the **first prompt**; expected reply (rendered transcript).
7. Permissions prompts you'll see ("allow running commands?") and what to answer.
8. Troubleshooting (game not running, wrong port, photo mode, driving).

### Starter kit
- Download (zip from GitHub Releases) **and** copy-paste view.
- Contents: `AGENT-INSTRUCTIONS.md` (works as AGENTS.md / CLAUDE.md / GEMINI.md /
  Cursor rules — same text, multiple filenames), optional Python client, a
  `prompts.md` of starter prompts.
- What the instructions tell the agent (distilled from our hard-won lessons):
  endpoint & request format; **start with `world.help`**; read before writing;
  **save the game first** and confirm; dry-run (`test*`) before building; verify
  connections after building; check photo mode / driving before building; never
  teleport the player into void/deep water — perch on a placed foundation; ask
  before creative or destructive actions; report what it changed.

### First conversation
A guided script with expected replies: (1) "Are you connected? Tell me the game
version." (2) "What's around me?" (3) "Save my game as `before-agent`."
(4) "Place a single foundation next to me" (first write — teaches camera swing).
(5) "Remove it." Celebrate → link to Simple tutorials.

### Safety
Plain-language version of the disclosures + rpc-player-interaction.md:
what it does to your save, character, camera, achievements; creative toggle;
networking; multiplayer; how to roll back (load your backup).

### FAQ / Troubleshooting
Agent says it can't connect · builds fail with "hologram" errors (photo mode!) ·
my position reads 0,0,0 (you're in a vehicle) · it built in the wrong place ·
belts say success but aren't attached · it's slow/expensive (model costs) · can it
play for me while I'm AFK (yes, with care) · does it work on Experimental/Epic/
dedicated servers · multiplayer.

### Developers
Architecture diagram (from README), link to generated RPC catalog, `world.help`,
Python client, contributing, issue tracker. This is where "RPC/JSON" words live.

## 6. "Get started with AI agents" strategy

- **One golden path, fully screenshotted:** recommend **Claude Code** (desktop app)
  as the first-timer path — it's the agent this mod was developed and tested with,
  has a GUI (no terminal required), and handles local requests out of the box.
  Every click of this path gets a screenshot.
- **Alternates (lighter guides, same template):** OpenAI Codex, Google Gemini CLI,
  Cursor (agent mode), and a generic "any agent that can run commands" page.
  Each must be **re-verified at writing time** and given a "last tested" date.
- **Starter kit is the real onboarding product.** Because the mod self-describes
  (`world.help`), the agent only needs: where to connect, how to send a request,
  and our safety/working rules. One instructions file, many filenames.
- **Transcripts as HTML, not screenshots:** render example conversations with a
  styled transcript component (crisp, accessible, editable, identical across
  agents). Use real app screenshots only where UI clicks matter (install, sign-in,
  permission prompts).
- **Biggest lever for non-technical users — consider an MCP connector** (separate
  engineering project, §15): a small MCP server wrapping the local API would let
  chat-style apps with MCP support connect, and some (e.g. Claude Desktop) support
  one-click extension installs. That would turn "install a coding agent" into
  "install an extension." Plan the site so an "MCP" path can slot into /agents later.

## 7. Tutorials

Template per tutorial: **Goal · Difficulty · Time · You'll need (tier unlocks,
materials, game state) · Prompts (copy buttons) · What you'll see (screenshots) ·
Verify it worked · If something goes wrong · Next tutorial.**
Each tutorial lists the exact prompts *and* what the agent should do, so readers
can tell when their agent goes off track.

### Simple (no building or one small build; ~10 min)
1. **Ask about your world** — nodes near you, unpowered machines, what's in storage.
2. **Cinematic photo of your base** — time of day + free camera + high-res photo
   (teaches photo mode, and that building is paused during it).
3. **Your first build** — save, place a miner on a node + a smelter, belt them,
   connect power, verify it's producing.
4. **Tidy up** — find idle machines, set recipes / clock speeds, mark nodes on the map.

### Medium (a working line; 30–60 min)
5. **Copper wire & cable line** — node → miner → smelters → constructors →
   storage, powered and verified. (Proven build.)
6. **Pay off a HUB milestone** — produce/collect, pull shortfall from the
   Dimensional Depot, select, launch.
7. **Drone delivery route** — two ports, paired both ways, fuel and cargo.
8. **Truck route on autopilot** — path, stations, fuel, arm autopilot.
9. **Plan before you build** — have the agent compute machine counts & raw
   inputs for a target rate, price it, then build.

### Complex (multi-hour / multi-step; agent works semi-autonomously)
10. **Heavy Modular Frame factory, end to end** — plan → site selection → build in
    phases → power → verify full rate. (Proven build; reference saves exist.)
11. **A self-driving train line / loop** — stations facing the right way, track,
    power, locomotive, timetable; lessons on straight track & docking direction
    (from the perimeter-loop experiment).
12. **Megastructure art** — floating belt tornado / basket weave (creative,
    "because you can"; teaches platform-perch safety).
13. **Let it run a base while you're away** — save checkpoints, guardrails,
    reviewing what it changed.

## 8. Visual design direction

Mood: **industrial-optimist** — FICSIT-adjacent without copying Coffee Stain's
trademarks or UI. Dark-first, with a light theme.

- **Palette (proposal):** charcoal/graphite surfaces (`#14161a`, `#1d2026`),
  steel-gray borders, **safety-orange accent** for player/primary actions, **blueprint
  cyan** reserved for *agent* actions (so "what the AI did" is always visually
  distinct), success green / caution amber / danger red for status. Final tokens
  checked for WCAG AA contrast in both themes.
- **Type:** a condensed industrial display face for headings (e.g. Barlow Condensed
  or Rajdhani), a highly readable UI sans for body (e.g. Inter or IBM Plex Sans),
  a monospace for prompts. All via Google Fonts or self-hosted.
- **Motifs:** subtle blueprint grid backgrounds; conveyor-belt line dividers that
  "flow" between sections (CSS, respects reduced-motion); hazard-stripe accents
  used *only* on safety callouts; machine-tier style difficulty badges
  (Simple / Medium / Complex).
- **Signature component — Prompt → Result card:** a chat bubble (the prompt, in
  mono) beside the in-game screenshot it produced, with a small "what the agent
  did" list in blueprint cyan. Used on Home, Features, Showcase, Tutorials.
- **Other components:** hero with video/still; feature tiles; step lists with
  numbered screenshots; callouts (tip / heads-up / safety); agent comparison
  table; agent cards; tutorial cards with difficulty filter; copy-to-clipboard
  prompt blocks; before/after slider (empty land → factory); styled transcript
  viewer; gallery + lightbox; FAQ accordion; theme toggle.
- **Responsive & accessible:** works at phone width; alt text for every
  screenshot; captions; keyboard-navigable lightbox; reduced-motion respected.
- **Deliverable for this step:** a one-page visual design mockup (home hero +
  prompt→result card + tutorial card + callouts in both themes) for sign-off
  before building templates.

## 9. Screenshots & media plan

### Capture pipeline (the mod photographs itself)
Use the mod's own tools so shots are reproducible and consistent:
load the reference save → `setTimeOfDay` (golden hour / midday) →
`setPhotoCamera(x,y,z,pitch,yaw)` → `takePhoto` (high-res, HUD off) — or
`captureScreenshot` (with `showUI` for HUD shots). A small "shot list" script
stores each shot's save + camera pose, so any shot can be re-taken after a game
update. (Reminder: photo mode pauses building — capture *after* builds.)

Standards: capture 2560×1440, export WebP at 1600 w + 800 w (+ AVIF optional),
consistent time of day and color grade, no personal info (player names, Steam
overlay), HUD off except for "this is what you'll see" UI shots.

### Shot list (launch set)
| # | Shot | Source | Used on |
|---|---|---|---|
| 1 | Hero: sweeping agent-built factory | HMF factory save (pre-teardown) | Home hero, ficsit.app |
| 2 | Before/after: empty site → copper line | build fresh, 2 captures | Home, Tutorial 5 |
| 3 | Train looping the perimeter (clip) | `FULL-LOOP-DRIVABLE` save | Home, Showcase, Tutorial 11 |
| 4 | Floating belt tornado / basket weave | `tornados-and-weave` save | Showcase, Tutorial 12 |
| 5 | Drones in flight between ports | build fresh | Tutorial 7, Features |
| 6 | Truck on autopilot | build fresh | Tutorial 8 |
| 7 | Survey result marked on the map (markers) | any save | Tutorial 1 |
| 8 | Photo-mode cinematic | any showpiece | Tutorial 2 |
| 9 | Mod settings menu (toggles) | live UI | Install, Safety |
| 10 | Satisfactory Mod Manager install steps (×4–6) | live UI | Install |
| 11 | Agent app install/sign-in/permission prompts (×6–10 per golden path) | live UI | Agent guides |
| 12 | Per-tutorial step shots (3–6 each) | builds | Tutorials |
| 13 | Giant manta / Project Assembly spectacle | live / saves | Showcase |

### Video
Short loops (5–15 s) as muted autoplay MP4/WebM < ~5 MB each, poster image,
reduced-motion fallback to still. Longer walkthroughs (build time-lapses,
tutorial videos) hosted on YouTube and embedded click-to-load (privacy-friendly,
no player load until clicked).

## 10. Technical architecture (static)

- **Recommended:** **Astro** (static output) with **Starlight** for the docs-style
  sections (install, agents, tutorials, safety, developers). Gives shared layouts,
  Markdown/MDX content, built-in static search (Pagefind), dark/light theme, and
  zero server code. Marketing pages (Home, Showcase) as custom Astro pages using
  the same design tokens.
- **Alternative (no build tooling):** hand-written HTML/CSS + small vanilla JS —
  simplest hosting, but nav/footer duplicated across ~20 pages and no search.
  Fine for a first 5-page launch; harder to grow into tutorials.
- **Client-side JS only:** copy-to-clipboard, theme toggle, tutorial difficulty
  filter, gallery lightbox, before/after slider, transcript viewer, tutorial
  checklist progress saved in `localStorage`, optional "prompt builder" (pick a
  goal → assembles a starter prompt).
- **Considered and rejected: a live "is my game connected?" checker in the browser.**
  Letting a website call the game's local endpoint would require opening the mod
  to browser origins (CORS / private-network-access), which would let *any*
  website drive a visitor's game. The agent does the connection check instead.
- **Content single-sourcing:** site imports `docs/player-features.md`,
  `docs/rpc-player-interaction.md`, `docs/known-limitations.md`, and the
  generated RPC catalog at build time, so the repo stays the source of truth.
- **Downloads:** the starter kit zip is built by CI and attached to GitHub
  Releases; the site links to "latest release".
- **Location:** a `website/` folder in this repo (so it can import `docs/`
  directly), deployed by a GitHub Actions workflow. (Alternative: a separate
  small repo — cleaner, but docs would need syncing.)
- **Performance budget:** home page < 1.5 MB on first load, images lazy-loaded,
  fonts subset, Lighthouse ≥ 90 across the board.

## 11. Hosting, domain & launch

- **Host:** GitHub Pages (free, HTTPS, repo already on GitHub). Alternatives:
  Cloudflare Pages / Netlify (also static & free).
- **Domain-agnostic build:** the site URL and base path are set in **one config
  value**; launch on the default `daten.github.io/...` address, add a custom
  domain later via a `CNAME` + DNS record, no content changes.
- **Domain ideas** (if wanted): something brand-safe that doesn't lead with the
  game's trademark — check availability and Coffee Stain's fan-content guidance (§12).
- **Analytics (optional):** cookie-less, privacy-friendly (e.g. GoatCounter /
  Plausible) or none. No cookie banner needed if cookie-less.
- **Launch sequencing:** ficsit.app listing + site go live together; ficsit.app
  long description links to the site; announce on r/SatisfactoryGame, the
  Satisfactory Modding Discord, and GitHub Releases.
- **SEO:** per-page titles/descriptions, Open Graph/Twitter cards using the shot
  list, sitemap.xml, canonical URLs set once the domain is final.

## 12. Legal, brand & safety

- **Fan-project disclaimer** in the footer: not affiliated with or endorsed by
  Coffee Stain Studios; Satisfactory is their trademark.
- **Follow Coffee Stain's fan content / mod policy** for game screenshots, name
  usage, and any game assets; use our **own logo** (no FICSIT logo or official
  UI art).
- **AI-agent brands:** describe agents by name in text; only use vendor logos if
  their brand guidelines permit (default: text chips, no logos).
- **Disclosures stay prominent** (same content as README/ficsit.app): back up
  saves, not achievement-safe, creative off by default, loopback-only, single-player.
- **License:** GPLv3 noted with a link; site content license chosen separately
  (e.g. CC BY 4.0) if we want others to reuse tutorials.

## 13. Maintenance

- "Last tested with game version X / mod version Y / agent version Z" stamp on
  every install, agent, and tutorial page.
- CI link check + build on every push; reuse the existing `gen_rpc_catalog.py --check`
  so the developer reference never drifts.
- Shot-list script makes re-capturing screenshots after game updates cheap.
- A short "content owner" note per section so stale agent guides get refreshed
  (agent products change monthly).

## 14. Roadmap

| Phase | Scope | Exit criteria |
|---|---|---|
| **0 — Decide** | Answer §15; verify agent install flows; check fan-content policy | Decisions recorded in this doc |
| **1 — Content core** | Home, Features, Install, Safety/FAQ copy; **starter kit** file; First Conversation script; Simple tutorials 1–3 | Copy reviewed; starter kit works with golden-path agent |
| **2 — Design** | Visual mockup → design tokens & components; site skeleton; deploy to github.io | Mockup signed off; skeleton live |
| **3 — Media** | Capture shot list 1–9 with the shot script; golden-path agent screenshots | All launch pages have real imagery |
| **4 — Agents & tutorials** | Golden-path guide + 2 alternates; Medium tutorials 5–8; Showcase | A first-timer completes golden path + Tutorial 3 unaided (hallway test) |
| **5 — Launch** | Complex tutorials 10–11, polish, SEO, a11y pass; ship with ficsit.app listing | Lighthouse ≥ 90; listing + site live; announced |
| **6 — Later** | MCP connector path; custom domain; video walkthroughs; more tutorials; translations | — |

## 15. Open decisions

1. **MCP connector?** Building one would make the site's "never installed agent
   software" story far simpler (and widen agent support). It's a separate
   engineering project — do we want it before launch, after, or never?
2. **Static site generator:** Astro + Starlight (recommended) vs plain HTML/CSS/JS.
3. **Where the site lives:** `website/` in this repo (recommended, single-source
   docs) vs a separate repo.
4. **Golden-path agent:** Claude Code (recommended, it's what the mod is tested
   with) — and which 2–3 alternates we commit to testing and maintaining.
5. **Video hosting:** YouTube embeds vs self-hosted short loops only.
6. **Domain & name** for the site; whether to include "Satisfactory" in it
   (trademark/fan-policy check).
7. **Creative-feature visibility:** feature them openly (with toggle explanation)
   or keep them to a single "optional creative extras" line.
8. **Community channel:** GitHub Discussions/Issues only, or a Discord?
