# Plan: architecture, features, roadmap

## 1. Engine choice

| Option | Pros | Cons |
|---|---|---|
| **Electron (chosen)** | Full Chromium engine, cross-platform, TypeScript end to end, Chrome extensions via `electron-chrome-extensions`, fast iteration | Trails Chrome security patches by days to weeks; no Widevine DRM unless we use castLabs' Electron fork; partial extension API coverage |
| Chromium fork (Arc's way) | Full control, full extension support | 40+ GB checkout, hours-long builds, a dedicated team just to rebase. Not for an early OSS project |
| Firefox fork (Zen's way) | Gecko, great privacy story | No Chrome extensions; Gecko UI internals are hard |
| Native WebKit (Swift) | Lightest, best on Mac | macOS only, no Chrome extensions |

**Decision:** Electron + castLabs ECS (for Widevine, so Netflix/Spotify work). The UI layer is
kept independent of Electron APIs so a Chromium fork stays possible later.

## 2. Architecture

```
┌──────────────────────── Main process (Node) ────────────────────────┐
│ WindowManager   TabManager      SessionManager   Store (SQLite)       │
│ windows,        WebContentsView per profile →    sidebar tree,        │
│ layout          per tab, sleep/ session partition history (FTS5),     │
│                 discard         `persist:<id>`   settings, downloads  │
│ ExtensionHost   AdBlock         Boosts           Updater              │
│ (chrome ext.)   (Ghostery lib)  (CSS/JS inject)  (electron-updater)   │
└──────────▲──────────────────────────────────────────────▲─────────────┘
           │ typed IPC (contextBridge, one preload)       │ state patches
┌──────────┴───────── Chrome UI renderer (React) ────────┴─────────────┐
│ Sidebar · Command bar · Split View layout · Peek · Settings · Library │
└───────────────────────────────────────────────────────────────────────┘
┌──── Web content (one WebContentsView per live tab) ───┐
│ sandbox: true, contextIsolation, NO preload, no Node  │
└───────────────────────────────────────────────────────┘
```

**Rules**
- **Main process owns all state.** The UI renders it and sends intents (`tab.open`,
  `space.switch`); main applies them and pushes changes. One source of truth, no sync bugs.
- **Web pages get nothing.** No preload, sandboxed, context isolation, Electron fuses
  locked down, strict permission handler (camera, mic, location, notifications prompt per site).
- **Tabs are data first, views second.** A sidebar item exists without a live
  `WebContentsView`; views are created on activation and discarded when idle (memory).
- **Profile = session partition.** Cookies/storage are isolated per profile; Spaces point
  at a profile.

**Stack:** TypeScript (strict), Electron + electron-vite, React for the chrome UI,
`better-sqlite3` for storage, `electron-chrome-extensions`, `@ghostery/adblocker-electron`,
`electron-builder` for signed/notarized builds, Vitest + Playwright for tests, pnpm.

**Data model (simplified)**
```
Profile  { id, name, partition }
Space    { id, profileId, name, icon, theme: { gradient, mode } }
Item     { id, spaceId | null (favorite), parentId, kind: folder | tab,
           pinned, title, url, pinnedUrl, order, lastActiveAt }
History  { url, title, visitedAt, profileId }   -- FTS5 index for the command bar
Rule     { pattern, spaceId }                    -- Air Traffic Control
Boost    { domain, css, js, zapSelectors, enabled }
```

## 3. Arc feature inventory

### Sidebar & tabs
- Vertical tab sidebar; can be hidden (`Cmd+S`) for full-bleed content
- **Favorites**: icon grid at the top, shared across Spaces of a profile, with badges (unread mail, etc.)
- **Pinned tabs** per Space, in nested **folders**; a pinned tab remembers its original URL and
  can reset back to it
- **Today / unpinned tabs** auto-archive after 12h / 24h / 7d / 30d
- **Archive**: searchable list of auto-closed tabs, one-click restore
- Rename tabs, duplicate, drag & drop everywhere, multi-select
- Media controls in the sidebar for the playing tab; auto Picture-in-Picture on tab switch
- Live Folders (e.g. GitHub PRs, RSS) — feed-backed folders

### Spaces & Profiles
- **Spaces**: separate tab sets with name, icon and gradient theme; switch with `Ctrl+1..9` or swipe
- **Profiles**: separate logins/cookies/history/extensions; each Space belongs to a profile
- **Air Traffic Control**: route URLs opened from other apps to a chosen Space by rule

### Navigation & search
- **Command bar** (`Cmd+T`, `Cmd+L`): one box for URLs, search, open tabs, history,
  bookmarks, and commands ("New Space", "Toggle dark mode", "Copy URL")
- Site search shortcuts (type `yt` + Tab → search YouTube)
- Copy URL (`Cmd+Shift+C`), reopen closed tab (`Cmd+Shift+T`), back/forward with gestures
- Custom search engine, search suggestions (opt-in)

### Windows & views
- **Split View**: up to 4 tabs side by side or stacked, saved as a sidebar item
- **Peek**: links from pinned tabs/favorites open in an overlay instead of a new tab
- **Little Arc–style mini window**: quick throwaway window for links from other apps; promote it to a Space
- Developer mode: compact URL bar showing host/port, console shortcut for localhost

### Personalisation
- **Boosts**: per-site custom CSS/JS, font/colour tweaks, and "Zap" (click to hide elements)
- Theme picker: gradients, grain, light/dark per Space; app icon variants

### Library & tools
- Library: downloads, media, captures in one place
- Capture: screenshot an area or full page, annotate, copy
- Notes/Easels (whiteboard of clippings) — low priority

### Platform
- Chrome extensions (Chrome Web Store install)
- Ad/tracker blocker built in (Arc relied on extensions; we ship one)
- Import from Chrome, Safari, Firefox and **Arc** (`StorableSidebar.json`: Spaces, pinned tabs, folders)
- Password manager integration (system keychain + extension support); passkeys
- Sync across devices (end-to-end encrypted)
- Auto-update, signed builds on macOS, Windows, Linux

### AI ("Arc Max" equivalents) — opt-in, bring your own key or local model
- Tidy tab titles, tidy downloaded file names
- Hover-link preview summaries
- Ask about this page
- Auto-group/tidy tabs

## 4. Roadmap

| Phase | Scope | Exit criteria |
|---|---|---|
| **0. Foundation** ✅ | Repo, license, rules, plan | This commit |
| **1. Shell (MVP)** | Electron app, one window, one profile; sidebar with vertical tabs; command bar (URL + search + open tabs); back/forward/reload; persistence across restarts; history; downloads; keyboard shortcuts; CI (lint/typecheck/test/build) | Usable as a daily driver for basic browsing on macOS |
| **2. Arc core** | Pinned vs Today tabs + auto-archive + Archive; folders; Favorites; Spaces with themes; Profiles (partitions); Split View; Peek; mini window; reopen closed tab | A former Arc user feels at home |
| **3. Power** | Chrome extensions; ad blocker; Boosts + Zap; Air Traffic Control; importers (Arc first); site search; media controls + auto-PiP; Widevine | Switch-over without losing anything |
| **4. Ship** | Signed/notarized builds, auto-update, Windows + Linux, tab discarding/perf budget, opt-in crash reports, settings UI, onboarding | Public beta |
| **5. Later** | E2E-encrypted sync (CRDT), AI features, Library/Capture, Live Folders, Easels/Notes, mobile companion | — |

## 5. Risks

- **Security lag vs Chrome** — track Electron stable aggressively; automate bumps with Dependabot + CI.
- **Extension coverage** — some MV3 APIs missing; publish a compatibility list.
- **Memory** — many sidebar tabs must not mean many live renderers; discard is mandatory, not an optimisation.
- **Trademark** — never use the Arc name or assets in the product.
