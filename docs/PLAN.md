# Plan: architecture, features, roadmap

## 1. Engine: Chromium, built the Brave way

We build on Chromium (the open-source code behind Chrome) and keep our changes as a thin
layer on top, the way Brave does. We never edit a copy of Chromium in place, so moving to
each new Chromium release (every 4 weeks) stays a rebase of our layer, not a merge war.

Why not the alternatives:
- **Electron**: quick to start, but it trails Chrome's security fixes, supports only part of the
  extension APIs, and can't replace the browser's own window and tab strip.
- **Firefox (Zen's way)**: no Chrome extensions, a much smaller share of the web tests against Gecko.
- **Native WebKit**: macOS only.

Cost we accept: a ~100 GB checkout, multi-hour first builds, and a standing duty to track
Chromium stable.

## 2. Repository layout (Brave-style)

Brave splits this into `brave-browser` (build tooling) and `brave-core` (the code). We keep
both in one repo until it hurts. Like `brave-core`, the repo root is what sits at
`chromium/src/browser/`, so includes read `#include "browser/components/..."` (renamed
once the project has a name).

```
browser/                # repo root = chromium/src/browser/
├── browser/            # C++ glued into Chromium: sidebar Views, Spaces, command bar, split view…
├── components/         # C++ features; each has core/ (plain C++, no Chromium) + Chromium glue
├── chromium_src/       # override files that replace/extend Chromium sources at compile time
├── ui/webui/           # TypeScript WebUI pages: command bar, settings, library, onboarding
├── app/                # branding: name, icons, strings, theme
├── patches/            # small .patch files against Chromium; the last resort
├── scripts/            # fetch Chromium + depot_tools, apply patches, build, package, test_core.sh
├── CHROMIUM_VERSION    # the exact Chromium tag we build on
└── docs/
```

**Change rules, in order of preference**
1. New code in this repo behind our own GN targets — no Chromium file touched.
2. `chromium_src/` override — compile-time replacement of a single Chromium file, keeping the original reachable.
3. A small `.patch` in `patches/` — only for hooks we can't add any other way. Every patch is reviewed for size.

## 3. Architecture

```
┌────────────────────── Browser process (C++) ───────────────────────┐
│ Window frame (Views)                                                │
│  ├─ Sidebar (Views)  ← replaces the horizontal tab strip           │
│  ├─ Content area: 1 tab, or Split View (2–4 panes), or Peek overlay │
│  └─ Command bar (WebUI, TypeScript)  ← replaces the omnibox popup   │
│                                                                     │
│ Per-profile services (KeyedService)                                 │
│  SpacesService  SidebarModel  ArchiveService  BoostsService         │
│  AirTrafficControl  ShieldsService (adblock-rust)  Importers        │
│  Storage: SQLite in the profile directory                           │
└──────────▲───────────────────────────────────────────▲──────────────┘
           │ Mojo (typed IPC)                           │ Mojo
┌──────────┴─────────────┐               ┌─────────────┴───────────────┐
│ WebUI pages (TS)       │               │ Web pages (renderer)         │
│ command bar, settings, │               │ Chromium sandbox, untouched  │
│ library, archive       │               │ site isolation               │
└────────────────────────┘               └──────────────────────────────┘
```

**Key decisions**
- **Native sidebar (Views), WebUI for everything else.** The sidebar must handle drag & drop,
  hundreds of items and tab state at native speed, so it's C++ Views built on Chromium's
  `TabStripModel`. Command bar, settings, library and archive are WebUI (TypeScript), which
  is faster to build and easier for contributors.
- **Sidebar items are data; tabs are live pages.** Pinned tabs, folders and favorites live in
  our SQLite store. A real Chromium tab exists only when an item is open; idle tabs are
  discarded with Chromium's built-in tab discarding.
- **Profiles are Chromium profiles.** Cookies, history and extensions are isolated per profile for free.
- **A Space is a filtered view of one window's tabs**, plus its own pinned items and theme.
  Switching Spaces swaps which tabs the sidebar shows; it doesn't open a new window.
- **Privacy like Brave, without crypto/ads.** Use Brave's open-source `adblock-rust` engine
  (MPL-2.0) for ads/trackers. Ship with Google sign-in, sync and telemetry removed, using
  ungoogled-chromium's patches as a reference. Proxy the Google services we keep (Safe
  Browsing) so they don't identify users.

**Languages & tools:** C++ (Chromium style, `clang-format`), TypeScript for WebUI, GN/Ninja,
Python for build scripts, Node for the scripts wrapper. Tests: Chromium's `unit_tests` /
`browser_tests` targets for our code, plus WebUI tests.

**Data model (simplified)**
```
Profile  = Chromium profile
Space    { id, name, icon, theme: { gradient, mode } }
Item     { id, spaceId | null (favorite), parentId, kind: folder | tab | split,
           pinned, title, url, pinnedUrl, order, lastActiveAt }
Rule     { pattern, spaceId }                    -- Air Traffic Control
Boost    { domain, css, js, zapSelectors, enabled }
History  = Chromium history (already full-text searchable)
```

## 4. Arc feature inventory

### Sidebar & tabs
- Vertical tab sidebar; can be hidden (`Cmd+S`) for full-bleed content
- **Favorites**: icon grid at the top, shared across a profile's Spaces, with badges (unread mail, etc.)
- **Pinned tabs** per Space, in nested **folders**; a pinned tab remembers its original URL and can reset to it
- **Today / unpinned tabs** auto-archive after 12h / 24h / 7d / 30d
- **Archive**: searchable list of auto-closed tabs, one-click restore
- Rename tabs, duplicate, drag & drop everywhere, multi-select
- Media controls in the sidebar for the playing tab; auto Picture-in-Picture on tab switch
- Live Folders (e.g. GitHub PRs, RSS) — feed-backed folders

### Spaces & Profiles
- **Spaces**: separate tab sets with name, icon and gradient theme; switch with `Ctrl+1..9` or swipe
- **Profiles**: separate logins/cookies/history/extensions; each Space belongs to a profile
- **Air Traffic Control**: route links opened from other apps to a chosen Space by rule

### Navigation & search
- **Command bar** (`Cmd+T`, `Cmd+L`): one box for URLs, search, open tabs, history,
  bookmarks and commands ("New Space", "Toggle dark mode", "Copy URL")
- Site search shortcuts (type `yt` + Tab → search YouTube)
- Copy URL (`Cmd+Shift+C`), reopen closed tab (`Cmd+Shift+T`), swipe back/forward
- Choice of search engine, search suggestions (opt-in)

### Windows & views
- **Split View**: up to 4 tabs side by side or stacked, saved as one sidebar item
- **Peek**: links from pinned tabs/favorites open in an overlay instead of a new tab
- **Mini window** (Arc's "Little Arc"): quick throwaway window for links from other apps; promote it to a Space
- Developer mode: compact URL bar showing host/port

### Personalisation
- **Boosts**: per-site custom CSS/JS, font/colour tweaks, and "Zap" (click to hide elements)
- Theme picker: gradients, grain, light/dark per Space; app icon variants

### Privacy (Brave-style "Shields")
- Built-in ad and tracker blocking (adblock-rust + standard filter lists), per-site toggle
- Automatic HTTPS upgrade, cookie and referrer tightening, fingerprinting protections
- No Google account, no telemetry by default; crash reports opt-in

### Library & tools
- Library: downloads, media, captures in one place
- Capture: screenshot an area or full page, annotate, copy
- Notes/Easels (whiteboard of clippings) — low priority

### Platform
- Chrome Web Store extensions (native Chromium support)
- Import from Chrome, Safari, Firefox and **Arc** (`StorableSidebar.json`: Spaces, pinned tabs, folders)
- Passwords and passkeys (Chromium's password manager)
- Sync across devices — our own end-to-end encrypted server (Brave runs its own `go-sync`)
- Auto-update (Sparkle on macOS, Omaha on Windows, package repos on Linux)

### AI ("Arc Max" equivalents) — opt-in, bring your own key or a local model
- Tidy tab titles and downloaded file names
- Hover-link preview summaries
- Ask about this page
- Auto-group/tidy tabs

## 5. Developing without a Chromium build (current mode)

Until we have a machine that builds Chromium, every feature is split so most of it can be
written and tested on any laptop:

| Layer | Where | How we test it now |
|---|---|---|
| **Core logic**: sidebar tree, auto-archive rules, Spaces, Air Traffic Control matching, Boost matching, Arc/Chrome importers | `components/<feature>/core/`, plain C++20 (standard library only, no Chromium `base::`, no `<chrono>`) | GoogleTest unit tests (same files Chromium's `unit_tests` will run), via `scripts/test_core.sh` |
| **WebUI pages**: command bar, settings, library, archive | `ui/webui/<page>/`, TypeScript + Lit (what Chromium's WebUI uses) | Runs in any browser against a fake backend that returns sample data |
| **Chromium integration**: Views sidebar, KeyedServices, Mojo interfaces, `chromium_src/` overrides, patches | `browser/`, `components/<feature>/` (outside `core/`), `chromium_src/`, `patches/` | Written against Chromium's API at `CHROMIUM_VERSION`; **not compiled yet**. Kept thin: it only connects the core logic to Chromium. |

Rules for this mode:
- Core logic must not include Chromium headers, so it compiles without Chromium.
  Integration code converts between Chromium types (`GURL`, `base::Time`) and core types.
- Integration code is reviewed by reading Chromium's source at our pinned version, since we can't compile it.
- When a build machine is available (new laptop, or a rented cloud machine for a day), the
  first job is compiling all integration code and fixing what breaks.

## 6. Roadmap

| Phase | Scope | Exit criteria |
|---|---|---|
| **0. Foundation** ✅ | Repo, license, rules, plan | Done |
| **1. Fork tooling** | `scripts/` to fetch Chromium at `CHROMIUM_VERSION`, apply patches, build, package; our name and icons; Google services off; CI checks that patches still apply | A branded build runs on macOS from one command |
| **2. Shell** | Sidebar replaces the tab strip; command bar; hidden toolbar; pinned vs Today tabs; session restore | Daily-driver for basic browsing on macOS |
| **3. Arc core** | Auto-archive + Archive; folders; Favorites; Spaces with themes; Profiles in Spaces; Split View; Peek; mini window | A former Arc user feels at home |
| **4. Power & privacy** | Shields (adblock-rust); Boosts + Zap; Air Traffic Control; importers (Arc first); site search; media controls + auto-PiP; Widevine | Switch-over without losing anything |
| **5. Ship** | Signed/notarized builds, auto-update, Windows + Linux, perf budget, settings UI, onboarding | Public beta |
| **6. Later** | E2E-encrypted sync, AI features, Library/Capture, Live Folders, Easels/Notes, mobile | — |

## 7. Risks

- **Chromium rebases every 4 weeks.** Keep the patch count low (rules in §2) and track stable on a schedule.
- **Build infrastructure.** Hosted CI runners can't build Chromium in time. CI on pull requests runs
  lint + "patches apply" + WebUI tests; full builds run on self-hosted machines with a shared build cache.
- **Widevine (Netflix, Spotify).** Needs a license agreement with Google and signed builds;
  start the application early.
- **Google API keys.** Without them some features (translation, location, Safe Browsing) need
  replacements or our own proxy.
- **Manifest V2 is gone upstream.** Shields is built in, so we don't depend on MV2 ad blockers.
- **Trademark.** Never use the Arc or Brave names or assets in the product.
