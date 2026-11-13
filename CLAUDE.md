# CLAUDE.md

Open-source, sidebar-first browser inspired by Arc, built on Chromium the way Brave is
(our code as a thin layer on top of upstream Chromium). Read `docs/PLAN.md` first: it is
the source of truth for architecture, features and roadmap.

## Status

Phase 2 groundwork (core logic). **We are not building Chromium locally yet** (no capable machine).
Write features following `docs/PLAN.md` §5: core logic in plain C++20 with no Chromium
headers (compile and test it with the system `clang++`), WebUI pages in TypeScript tested
in a normal browser against a fake backend, and Chromium integration code kept thin and
checked by reading Chromium's source at `CHROMIUM_VERSION`. Never claim integration code
compiles; say it is unbuilt.

## Commands

- `scripts/test_core.sh` — builds all `components/*/core` code and tests into one binary
  (`out/core/core_unittests`, extra args go to gtest, e.g. `--gtest_filter=Arc*`) with
  the system clang++ (ASan + UBSan, `-Werror`). Needs `brew install googletest`.
- `pnpm install`, then: `pnpm test` (WebUI logic, `node --test` on `ui/**/*.test.ts`),
  `pnpm typecheck` (tsc, strict), `pnpm dev` (command bar in a normal browser with
  FakeBackend).

## Layout

Repo root is the overlay that lives at `chromium/src/browser/` (like `brave-core`), so
includes are `"browser/components/..."` and gtest is `"testing/gtest/include/gtest/gtest.h"`;
`test_core.sh` fakes both include roots with symlinks in `out/core/include/`.

- `components/sidebar/core/` — SidebarModel: Favorites, per-Space pinned tree + Today
  tabs, folders, Split Views (2–4 tabs, dissolve at 1), Space name/icon/theme,
  pin/unpin, pinned-URL reset, auto-archive and restore.
- `components/importer/core/` — Arc importer: parses `StorableSidebar.json` (own small JSON
  parser) into one SidebarModel per Arc profile, including Split Views and Space themes.
- `ui/webui/command_bar/` — Arc-style command bar (Lit). `backend.ts` is the future Mojo
  interface + FakeBackend; `command_bar_logic.ts` holds URL-vs-search, command matching
  and result order (tested). Lit elements use `static properties` + `declare` fields and
  no decorators (Chromium WebUI style, and node can strip the types). Import with `.ts`
  extensions.

## Stack

Chromium (version pinned in `CHROMIUM_VERSION`), C++ for browser UI (Views) and services,
TypeScript for WebUI pages, Mojo for IPC, GN/Ninja, Python/Node build scripts, adblock-rust
for Shields.

## Conventions

- Change Chromium in this order of preference: new code in this repo → `chromium_src/`
  override → small `.patch` in `patches/`. Patch count is a cost; keep it low.
- C++ follows the Chromium style guide (`clang-format`, `gn format`).
- Conventional Commits, signed off (`git commit -s`), squash-merge PRs into `main`.
- Any new Mojo interface, WebUI binding or permission must be called out in the PR for security review.
- Never use the "Arc" or "Brave" names or assets in the product (trademarks).
- License is MPL-2.0; new source files carry the MPL-2.0 header.
- Contributor rules: `CONTRIBUTING.md`. Security process: `SECURITY.md`.
